#pragma once
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "sync_trampoline.h"

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
    AudioHookBuffer buffers[32];
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
    if (!out || !out->stream) return 0;
    int queued = SDL_GetAudioStreamQueued(out->stream);
    uint64_t q_frames = (queued > 0) ? (uint64_t)queued / (out->channels * 2u) : 0;
    return out->appended_frames > q_frames ? out->appended_frames - q_frames : 0;
}

static void SDLCALL extra_audio_stream_cb(void *userdata, SDL_AudioStream *stream, int add, int tot) {
    (void)add; (void)tot;
    AudioHookOutput *out = (AudioHookOutput *)userdata;
    if (!out || !out->running || out->count == 0) return;
    int queued = SDL_GetAudioStreamQueued(stream);
    uint64_t q_frames = (queued > 0) ? (uint64_t)queued / (out->channels * 2u) : 0;
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
    if (!out || !out->stream) return 0;
    SDL_PauseAudioStreamDevice(out->stream);
    out->running = 0;
    return 1;
}

static inline int extra_audio_append(AudioHookOutput *out, uint64_t tag, const void *pcm, size_t size) {
    if (!out || !out->stream || !out->ready || out->count >= 32) return 0;
    size_t align = out->channels * 2u;
    if (!align || (size % align) != 0) return 0;

    SDL_LockAudioStream(out->stream);
    size_t skip = 0;
    if (out->running) {
        static int resync = -1;
        if (resync < 0) {
            const char *v = getenv("AC_AUDIO_RESYNC");
            resync = !(v && (v[0] == '0' || v[0] == 'n' || v[0] == 'N'));
        }
        double now = (double)extra_qpc() / 1e9;
        uint64_t played = extra_audio_position(out);
        uint64_t queued = out->appended_frames > played ? out->appended_frames - played : 0;
        if (queued) out->play_end = now + (double)queued / (double)out->rate;
        else if (out->play_end > now) out->play_end = now;

        if (out->play_end > 0 && now > out->play_end) {
            double gap = now - out->play_end;
            out->starved += gap;
            if (resync && gap < 0.5) {
                out->owed += gap;
                if (out->owed > 0.3) out->owed = 0.3;
            }
        }
        if (out->owed > 0) {
            size_t byte_rate = out->rate * align;
            size_t owed = (size_t)(out->owed * byte_rate) / align * align;
            size_t most = (size / 4 * 3) / align * align;
            skip = owed < most ? owed : most;
            out->owed -= (double)skip / (double)byte_rate;
            if (out->owed < 0.0005) out->owed = 0;
            out->skipped += (double)skip / (double)byte_rate;
            static unsigned s_resync_log = 0;
            if (skip > 0 && s_resync_log++ < 15) printf("[audio] resync: dropped %zu bytes to realign (owed=%.1fms)\n", skip, out->owed * 1000.0);
        }
        out->queued_then = out->play_end > now ? out->play_end - now : 0;
        out->last_append = now;
        if (out->play_end < now) out->play_end = now;
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

    unsigned idx = (out->head + out->count) % 32;
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
    if (!out || !out->stream) return 0;
    SDL_LockAudioStream(out->stream);
    uint64_t played = extra_audio_position(out);
    unsigned collected = 0;
    while (collected < cap && out->count > 0) {
        AudioHookBuffer *buf = &out->buffers[out->head];
        if (played < buf->end_frame) break;
        tags[collected++] = buf->tag;
        out->queued_bytes -= buf->length;
        out->head = (out->head + 1) % 32;
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
    if (!out) return;
    if (out->stream) {
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

typedef AudioHookOutput WaveOutput;
typedef int MMRESULT;
#define MMSYSERR_NOERROR 0
#define MMSYSERR_ALLOCATED 4
#define MMSYSERR_INVALHANDLE 5
#define MMSYSERR_NODRIVER 6
#define MMSYSERR_NOMEM 7
#define MMSYSERR_NOTSUPPORTED 8
#define MMSYSERR_INVALPARAM 11
#define WAVERR_BADFORMAT 32

static inline MMRESULT wave_output_open(WaveOutput *output, unsigned rate, unsigned channels) {
    return extra_audio_open(output, rate, channels) ? MMSYSERR_NOERROR : MMSYSERR_NODRIVER;
}
static inline MMRESULT wave_output_start(WaveOutput *output) {
    return extra_audio_start(output) ? MMSYSERR_NOERROR : MMSYSERR_INVALHANDLE;
}
static inline MMRESULT wave_output_pause(WaveOutput *output) {
    return extra_audio_pause(output) ? MMSYSERR_NOERROR : MMSYSERR_INVALHANDLE;
}
static inline MMRESULT wave_output_append(WaveOutput *output, uint64_t tag, const void *pcm, size_t size) {
    return extra_audio_append(output, tag, pcm, size) ? MMSYSERR_NOERROR : MMSYSERR_NOMEM;
}
static inline MMRESULT wave_output_collect(WaveOutput *output, uint64_t *tags, unsigned capacity, unsigned *count) {
    *count = extra_audio_collect(output, tags, capacity);
    return MMSYSERR_NOERROR;
}
static inline MMRESULT wave_output_position(WaveOutput *output, uint64_t *frames) {
    *frames = extra_audio_position(output);
    return MMSYSERR_NOERROR;
}
static inline unsigned wave_output_abandon(WaveOutput *output, uint64_t *dropped, unsigned capacity) {
    unsigned n = 0;
    for (unsigned i = 0; i < output->count && n < capacity; i++) {
        dropped[n++] = output->buffers[(output->head + i) % 32].tag;
    }
    extra_audio_close(output);
    return n;
}
static inline MMRESULT wave_output_close(WaveOutput *output) {
    extra_audio_close(output);
    return MMSYSERR_NOERROR;
}
