// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] SDL3 audio stream registry and volume/gain synchronization.
#pragma once
#include <SDL3/SDL.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "additions/toggles/ac_toggles.h"

#define EXTRA_AUDIO_MAX_STREAMS 16
#define EXTRA_AUDIO_GAIN_PERCENT_DIV 100.0f

static atomic_flag s_extra_audio_lock = ATOMIC_FLAG_INIT;
static SDL_AudioStream *s_extra_audio_streams[EXTRA_AUDIO_MAX_STREAMS] = {NULL};

static inline void extra_audio_lock_acq(void) {
    while (atomic_flag_test_and_set_explicit(&s_extra_audio_lock, memory_order_acquire)) {
    }
}

static inline void extra_audio_lock_rel(void) {
    atomic_flag_clear_explicit(&s_extra_audio_lock, memory_order_release);
}

static inline float extra_audio_effective_gain(void) {
    int vol = g_extra_audio_volume;
    if (vol < 0) {
        const char *ev = getenv("AC_MASTER_VOLUME");
        vol = (ev && *ev) ? atoi(ev) : EXTRA_AUDIO_VOL_MAX;
    }
    if (vol < 0) {
        vol = 0;
    }
    if (vol > EXTRA_AUDIO_VOL_MAX) {
        vol = EXTRA_AUDIO_VOL_MAX;
    }
    int mute_unfocus = g_extra_audio_mute_unfocus;
    if (mute_unfocus < 0) {
        const char *em = getenv("AC_MUTE_ON_UNFOCUS");
        mute_unfocus = (em && *em && strcmp(em, "0") != 0 && strcmp(em, "false") != 0) ? 1 : 0;
    }
    if (mute_unfocus && !g_sdl_input_focus) {
        return 0.0f;
    }
    return (float)vol / EXTRA_AUDIO_GAIN_PERCENT_DIV;
}

static inline void extra_audio_sync_gain(void) {
    static uint32_t s_last_gen = 0;
    static int s_last_focus = -1;
    uint32_t cur_gen = g_extra_audio_gen;
    int cur_focus = g_sdl_input_focus;
    if (cur_gen == s_last_gen && cur_focus == s_last_focus) {
        return;
    }
    s_last_gen = cur_gen;
    s_last_focus = cur_focus;
    float gain = extra_audio_effective_gain();
    extra_audio_lock_acq();
    for (int i = 0; i < EXTRA_AUDIO_MAX_STREAMS; i++) {
        if (s_extra_audio_streams[i]) {
            SDL_SetAudioStreamGain(s_extra_audio_streams[i], gain);
        }
    }
    extra_audio_lock_rel();
}

static inline void extra_audio_register_stream(SDL_AudioStream *stream) {
    if (!stream) {
        return;
    }
    float gain = extra_audio_effective_gain();
    extra_audio_lock_acq();
    for (int i = 0; i < EXTRA_AUDIO_MAX_STREAMS; i++) {
        if (!s_extra_audio_streams[i]) {
            s_extra_audio_streams[i] = stream;
            break;
        }
    }
    SDL_SetAudioStreamGain(stream, gain);
    extra_audio_lock_rel();
}

static inline void extra_audio_unregister_stream(SDL_AudioStream *stream) {
    if (!stream) {
        return;
    }
    extra_audio_lock_acq();
    for (int i = 0; i < EXTRA_AUDIO_MAX_STREAMS; i++) {
        if (s_extra_audio_streams[i] == stream) {
            s_extra_audio_streams[i] = NULL;
        }
    }
    extra_audio_lock_rel();
}
