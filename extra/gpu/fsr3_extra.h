/* [extra] AMD FSR 3.1 runtime helpers: Halton(2,3) camera jitter, 3D scene pass
 * filter, negative MIP LOD bias, RCAS attenuation, and FrameGen pacing. */
#pragma once

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <sched.h>
#include "extra/toggles/ac_toggles.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Radix-inverse Halton sequence centered in (-0.5, +0.5) for index >= 1. */
static inline float extra_fsr3_halton(uint32_t index, uint32_t base) {
    float f = 1.0f, r = 0.0f;
    uint32_t i = index;
    while (i > 0) {
        f /= (float)base;
        r += f * (float)(i % base);
        i /= base;
    }
    return r - 0.5f;
}

/* FSR 3.1 recommended Halton phase count: 8 * (out_w / render_w)^2, quantized to {16, 32, 64}. */
static inline uint32_t extra_fsr3_phase_count(uint32_t render_w, uint32_t out_w) {
    if (render_w == 0 || out_w <= render_w) return 16u;
    float ratio = (float)out_w / (float)render_w;
    float raw = 8.0f * ratio * ratio;
    if (raw <= 20.0f) return 16u;
    if (raw <= 40.0f) return 32u;
    return 64u;
}

/* Returns 1 only for 16:9 3D scene passes with depth testing enabled.
 * Rejects square shadow maps (e.g. 1024x1024), 2D UI/HUD passes (no depth),
 * and passes drawing directly to a presented swap target. */
static inline int extra_fsr3_should_jitter_pass(int has_depth, uint32_t rt_w, uint32_t rt_h,
                                                uint32_t depth_st0, int is_presented) {
    if (!has_depth || depth_st0 == 0 || is_presented) return 0;
    if (rt_w < 480 || rt_h < 270) return 0;
    float aspect = (float)rt_w / (float)rt_h;
    return (aspect >= 1.60f && aspect <= 1.90f) ? 1 : 0;
}

/* Negative MIP LOD bias so 3D samplers select sharper mipmaps matching the output resolution. */
static inline float extra_fsr3_compute_mip_bias(float upscale_ratio, int jitter_active) {
    if (!jitter_active) return 0.0f;
    float r = upscale_ratio > 1.0f ? upscale_ratio : 1.0f;
    float bias = -log2f(r) - 0.45f;
    if (bias < -2.5f) bias = -2.5f;
    return bias;
}

/* Maps user sharpness [0.0 .. 1.0] to FSR 3.1 RCAS linear lobe scale. */
static inline float extra_fsr3_rcas_scale(float sharpness) {
    if (sharpness <= 0.001f) return 0.0f;
    if (sharpness > 1.0f) sharpness = 1.0f;
    float stops = (1.0f - sharpness) * 1.25f;
    return exp2f(-stops);
}

#ifndef FSR3_MATH_ONLY
#include "settings.h"

extern __thread uint64_t g_blocked_ns;

static inline int extra_fsr3_eff_jitter(void) {
    if (g_settings.upscaler != UPSCALE_FSR3) return 0;
    return (g_extra_fsr_jitter >= 0) ? g_extra_fsr_jitter : g_settings.fsr_jitter;
}

static inline int extra_fsr3_eff_framegen(void) {
    return (g_extra_framegen >= 0) ? g_extra_framegen : g_settings.framegen;
}

static inline float extra_fsr3_eff_sharpness(void) {
    return (g_extra_fsr_sharpness >= 0.0f) ? g_extra_fsr_sharpness : g_settings.fsr_sharpness;
}

static inline float extra_fsr3_eff_render_scale(void) {
    float s = (g_extra_render_scale > 0.0f) ? g_extra_render_scale : g_settings.render_scale;
    return (s >= 0.5f && s <= 2.0f) ? s : 1.0f;
}

static inline float extra_fsr3_mip_bias(void) {
    if (!extra_fsr3_eff_jitter()) return 0.0f;
    return -0.25f;
}

/* Advances the per-frame Halton(2,3) camera jitter state at the end/start of a game frame. */
static inline void extra_fsr3_frame_step(uint32_t render_w, uint32_t out_w) {
    g_extra_fsr3.prev_jitter_px[0] = g_extra_fsr3.jitter_px[0];
    g_extra_fsr3.prev_jitter_px[1] = g_extra_fsr3.jitter_px[1];
    g_extra_fsr3.upscale_ratio = (render_w > 0 && out_w > 0) ? ((float)out_w / (float)render_w) : 1.0f;
    g_extra_fsr3.jitter_active = extra_fsr3_eff_jitter();
    if (g_extra_fsr3.jitter_active) {
        uint32_t phases = extra_fsr3_phase_count(render_w, out_w);
        uint32_t idx = (g_extra_fsr3.phase_index % phases) + 1u;
        g_extra_fsr3.phase_index++;
        g_extra_fsr3.jitter_px[0] = extra_fsr3_halton(idx, 2u);
        g_extra_fsr3.jitter_px[1] = extra_fsr3_halton(idx, 3u);
    } else {
        g_extra_fsr3.jitter_px[0] = 0.0f;
        g_extra_fsr3.jitter_px[1] = 0.0f;
    }
}

/* Midpoint pacing for Frame Generation: paces Present #2 exactly halfway through
 * the real frame cycle to eliminate frametime judder/sawtooth, and credits g_blocked_ns
 * so vk_pace_frame() never falsely triggers the 30 FPS fallback. */
static inline void extra_fsr3_fg_pace_midpoint(uint64_t target_ns) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
    if (now >= target_ns) return;
    uint64_t rem_ns = target_ns - now;
    if (rem_ns > 20000000ull) rem_ns = 20000000ull; /* safety clamp: 20 ms */
    uint64_t t_start = now;
    if (rem_ns > 350000ull) {
        uint64_t coarse_ns = rem_ns - 250000ull;
        struct timespec req = {(time_t)(coarse_ns / 1000000000ull), (long)(coarse_ns % 1000000000ull)};
        nanosleep(&req, NULL);
    }
    for (;;) {
        clock_gettime(CLOCK_MONOTONIC, &ts);
        uint64_t cur = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
        if (cur >= target_ns) break;
        #if defined(__x86_64__) || defined(_M_X64)
        __builtin_ia32_pause();
        #else
        sched_yield();
        #endif
    }
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t end = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
    g_blocked_ns += (end > t_start ? (end - t_start) : 0);
}

static inline void extra_fsr3_fg_pace_half(uint64_t busy_so_far_ns) {
    (void)busy_so_far_ns;
}
#endif /* FSR3_MATH_ONLY */

#ifdef __cplusplus
}
#endif
