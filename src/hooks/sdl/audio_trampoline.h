#pragma once
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include "hooks/platform/sync_trampoline.h"
#include "additions/toggles/ac_toggles.h"

#include "hooks/sdl/audio_stream_mgr.h"

#define EXTRA_AUDIO_QUEUE_CAP 32
#define EXTRA_AUDIO_CHANNELS_ALIGN_MULT 2u
#define EXTRA_AUDIO_RESYNC_MAX_GAP_SEC 0.5
#define EXTRA_AUDIO_RESYNC_MAX_OWED_SEC 0.3
#define EXTRA_AUDIO_RESYNC_MIN_OWED_SEC 0.0005
#define EXTRA_AUDIO_RESYNC_LOG_LIMIT 15
#define EXTRA_AUDIO_SKIP_FRACTION_NUM 3
#define EXTRA_AUDIO_SKIP_FRACTION_DEN 4
#define EXTRA_AUDIO_MS_PER_SEC 1000.0
#define EXTRA_AUDIO_SEC_PER_NSEC 1e-9

static _Atomic double g_extra_audio_starved_sec = 0.0;

static inline void extra_audio_add_starved(double gap) {
    if (gap > 0.0) {
        double cur = atomic_load_explicit(&g_extra_audio_starved_sec, memory_order_relaxed);
        while (!atomic_compare_exchange_weak_explicit(
            &g_extra_audio_starved_sec,
            &cur,
            cur + gap,
            memory_order_release,
            memory_order_relaxed
        ));
    }
}

static inline double extra_audio_consume_starved_ms(void) {
    double prev = atomic_exchange_explicit(&g_extra_audio_starved_sec, 0.0, memory_order_acq_rel);
    return prev * 1000.0;
}

typedef struct AudioHookBuffer {
    uint64_t tag;
    size_t length;
    uint64_t end_frame;
    struct {
        uint32_t dwBufferLength;
        const void *lpData;
    } header;
} AudioHookBuffer;
typedef AudioHookBuffer WaveBuffer;

typedef struct AudioHookOutput {
    SDL_AudioStream *stream;
    HANDLE completion;
    HANDLE closed;
    unsigned rate;
    unsigned channels;
    int running;
    int ready;
    struct {
        unsigned nBlockAlign;
        unsigned nSamplesPerSec;
        unsigned nChannels;
    } format;
    AudioHookBuffer buffers[EXTRA_AUDIO_QUEUE_CAP];
    unsigned head;
    unsigned count;
    uint64_t queued_bytes;
    uint64_t appended_frames;
    double play_end;
    double starved;
    double owed;
    double skipped;
    double last_append;
    double queued_then;
} AudioHookOutput;

static inline uint64_t extra_audio_position(AudioHookOutput *out) {
    if (!out || !out->stream) {
        return 0;
    }
    int queued = SDL_GetAudioStreamQueued(out->stream);
    uint64_t q_frames = (queued > 0) ? (uint64_t)queued / (out->channels * EXTRA_AUDIO_CHANNELS_ALIGN_MULT) : 0;
    return out->appended_frames > q_frames ? out->appended_frames - q_frames : 0;
}

static void SDLCALL extra_audio_stream_cb(void *userdata, SDL_AudioStream *stream, int add, int tot) {
    (void)add;
    (void)tot;
    AudioHookOutput *out = (AudioHookOutput *)userdata;
    if (!out || !out->running || out->count == 0) {
        return;
    }
    int queued = SDL_GetAudioStreamQueued(stream);
    uint64_t q_frames = (queued > 0) ? (uint64_t)queued / (out->channels * EXTRA_AUDIO_CHANNELS_ALIGN_MULT) : 0;
    uint64_t played = out->appended_frames > q_frames ? out->appended_frames - q_frames : 0;
    if (played >= out->buffers[out->head].end_frame) {
        extra_set_event(out->completion);
    }
}

static inline int extra_audio_open(AudioHookOutput *out, unsigned rate, unsigned channels) {
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    out->rate = rate;
    out->channels = channels;
    out->format.nSamplesPerSec = rate;
    out->format.nChannels = channels;
    out->format.nBlockAlign = channels * 2;
    out->completion = extra_create_event(1, 0);
    out->closed = extra_create_event(1, 0);

    SDL_AudioSpec spec = {
        .format = SDL_AUDIO_S16,
        .channels = (int)channels,
        .freq = (int)rate
    };
    out->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!out->stream) {
        fprintf(stderr, "[audio hook] SDL_OpenAudioDeviceStream failed: %s\n", SDL_GetError());
        return 0;
    }
    SDL_SetAudioStreamGetCallback(out->stream, extra_audio_stream_cb, out);
    extra_audio_register_stream(out->stream);
    out->ready = 1;
    const char *ev = getenv("AC_AUDIO_RESYNC");
    int resync = !(ev && (ev[0] == '0' || ev[0] == 'n' || ev[0] == 'N'));
    printf("[audio] SDL3 AudioStream open: %dHz %dch (resync=%d)\n", (int)rate, (int)channels, resync);
    return 1;
}

static inline int extra_audio_start(AudioHookOutput *out) {
    if (!out || !out->stream) return 0;
    SDL_ResumeAudioStreamDevice(out->stream);
    out->running = 1;
    out->play_end = 0;
    return 1;
}

static inline int extra_audio_pause(AudioHookOutput *out) {
    if (!out || !out->stream) {
        return 0;
    }
    SDL_PauseAudioStreamDevice(out->stream);
    out->running = 0;
    return 1;
}

static inline int extra_audio_append(AudioHookOutput *out, uint64_t tag, const void *pcm, size_t size) {
    if (!out || !out->stream || !out->ready || out->count >= EXTRA_AUDIO_QUEUE_CAP) {
        return 0;
    }
    extra_audio_sync_gain();
    size_t align = out->channels * EXTRA_AUDIO_CHANNELS_ALIGN_MULT;
    if (!align || (size % align) != 0) {
        return 0;
    }

    SDL_LockAudioStream(out->stream);
    size_t skip = 0;
    if (out->running) {
        static int resync = -1;
        if (resync < 0) {
            const char *v = getenv("AC_AUDIO_RESYNC");
            resync = !(v && (v[0] == '0' || v[0] == 'n' || v[0] == 'N'));
        }
        double now = (double)extra_qpc() * EXTRA_AUDIO_SEC_PER_NSEC;
        uint64_t played = extra_audio_position(out);
        uint64_t queued = out->appended_frames > played ? out->appended_frames - played : 0;
        if (queued) {
            out->play_end = now + (double)queued / (double)out->rate;
        } else if (out->play_end > now) {
            out->play_end = now;
        }

        if (out->play_end > 0 && now > out->play_end) {
            double gap = now - out->play_end;
            out->starved += gap;
            extra_audio_add_starved(gap);
            if (resync && gap < EXTRA_AUDIO_RESYNC_MAX_GAP_SEC) {
                out->owed += gap;
                if (out->owed > EXTRA_AUDIO_RESYNC_MAX_OWED_SEC) {
                    out->owed = EXTRA_AUDIO_RESYNC_MAX_OWED_SEC;
                }
            }
        }
        if (out->owed > 0) {
            size_t byte_rate = out->rate * align;
            size_t owed = (size_t)(out->owed * byte_rate) / align * align;
            size_t most = ((size / EXTRA_AUDIO_SKIP_FRACTION_DEN) * EXTRA_AUDIO_SKIP_FRACTION_NUM) / align * align;
            skip = owed < most ? owed : most;
            out->owed -= (double)skip / (double)byte_rate;
            if (out->owed < EXTRA_AUDIO_RESYNC_MIN_OWED_SEC) {
                out->owed = 0;
            }
            out->skipped += (double)skip / (double)byte_rate;
            static unsigned s_resync_log = 0;
            if (skip > 0 && s_resync_log < EXTRA_AUDIO_RESYNC_LOG_LIMIT) {
                s_resync_log++;
                printf("[audio] resync: dropped %zu bytes to realign (owed=%.1fms)\n",
                       skip, out->owed * EXTRA_AUDIO_MS_PER_SEC);
            }
        }
        out->queued_then = out->play_end > now ? out->play_end - now : 0;
        out->last_append = now;
        if (out->play_end < now) {
            out->play_end = now;
        }
        out->play_end += (double)(size - skip) / (double)(out->rate * align);
    }

    size_t length = size - skip;
    if (length > 0) {
        if (!SDL_PutAudioStreamData(out->stream, (const uint8_t *)pcm + skip, (int)length)) {
            SDL_UnlockAudioStream(out->stream);
            return 0;
        }
    }
    out->appended_frames += length / align;

    unsigned idx = (out->head + out->count) % EXTRA_AUDIO_QUEUE_CAP;
    out->buffers[idx].tag = tag;
    out->buffers[idx].length = length;
    out->buffers[idx].end_frame = out->appended_frames;
    out->buffers[idx].header.dwBufferLength = (uint32_t)length;
    out->buffers[idx].header.lpData = pcm;
    out->count++;
    out->queued_bytes += length;
    SDL_UnlockAudioStream(out->stream);
    return 1;
}

static inline unsigned extra_audio_collect(AudioHookOutput *out, uint64_t *tags, unsigned cap) {
    if (!out || !out->stream) {
        return 0;
    }
    SDL_LockAudioStream(out->stream);
    uint64_t played = extra_audio_position(out);
    unsigned collected = 0;
    while (collected < cap && out->count > 0) {
        AudioHookBuffer *buf = &out->buffers[out->head];
        if (played < buf->end_frame) {
            break;
        }
        tags[collected++] = buf->tag;
        out->queued_bytes -= buf->length;
        out->head = (out->head + 1) % EXTRA_AUDIO_QUEUE_CAP;
        out->count--;
    }
    extra_reset_event(out->completion);
    if (out->count > 0 && played >= out->buffers[out->head].end_frame) {
        extra_set_event(out->completion);
    }
    SDL_UnlockAudioStream(out->stream);
    return collected;
}

static inline void extra_audio_close(AudioHookOutput *out) {
    if (!out) {
        return;
    }
    if (out->stream) {
        extra_audio_unregister_stream(out->stream);
        SDL_DestroyAudioStream(out->stream);
        out->stream = NULL;
    }
    if (out->completion) {
        extra_close_handle(out->completion);
        out->completion = NULL;
    }
    if (out->closed) {
        extra_close_handle(out->closed);
        out->closed = NULL;
    }
    out->ready = 0;
    out->running = 0;
}

#include "hooks/sdl/audio_waveout_shim.h"
