// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] Win32 waveOut compatibility shim over SDL3 AudioStream.
#pragma once
#include <stdint.h>

typedef AudioHookOutput WaveOutput;
typedef int MMRESULT;

#define MMSYSERR_NOERROR       0
#define MMSYSERR_ALLOCATED     4
#define MMSYSERR_INVALHANDLE   5
#define MMSYSERR_NODRIVER      6
#define MMSYSERR_NOMEM         7
#define MMSYSERR_NOTSUPPORTED  8
#define MMSYSERR_INVALPARAM    11
#define WAVERR_BADFORMAT       32

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
        unsigned idx = (output->head + i) % EXTRA_AUDIO_QUEUE_CAP;
        dropped[n++] = output->buffers[idx].tag;
    }
    extra_audio_close(output);
    return n;
}

static inline MMRESULT wave_output_close(WaveOutput *output) {
    extra_audio_close(output);
    return MMSYSERR_NOERROR;
}
