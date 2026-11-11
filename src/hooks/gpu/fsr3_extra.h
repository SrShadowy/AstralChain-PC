/* [extra] AMD FSR 3.1 runtime helpers: Halton(2,3) camera jitter, 3D scene pass
 * filter, negative MIP LOD bias, RCAS attenuation, and FrameGen pacing. */
#pragma once

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <sched.h>
#include "additions/toggles/ac_toggles.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FSR3_HALTON_CENTER_OFFSET   0.5f
#define FSR3_HALTON_BASE_X          2u
#define FSR3_HALTON_BASE_Y          3u
#define FSR3_BASE_PHASE_MULT        8.0f
#define FSR3_PHASE_COUNT_MIN        16u
#define FSR3_PHASE_COUNT_MID        32u
#define FSR3_PHASE_COUNT_MAX        64u
#define FSR3_PHASE_THRESH_MID       20.0f
#define FSR3_PHASE_THRESH_MAX       40.0f
#define FSR3_MIN_SCENE_WIDTH        480u
#define FSR3_MIN_SCENE_HEIGHT       270u
#define FSR3_ASPECT_MIN_16_9        1.60f
#define FSR3_ASPECT_MAX_16_9        1.90f
#define FSR3_MIP_BIAS_OFFSET        0.45f
#define FSR3_MIP_BIAS_MIN          -2.5f
#define FSR3_MIP_BIAS_DEFAULT      -0.25f
#define FSR3_RCAS_MIN_SHARPNESS     0.001f
#define FSR3_RCAS_MAX_SHARPNESS     1.0f
#define FSR3_RCAS_STOP_SCALE        1.25f
#define FSR3_RENDER_SCALE_MIN       0.5f
#define FSR3_RENDER_SCALE_MAX       2.0f
#define FSR3_RENDER_SCALE_DEFAULT   1.0f
#define FSR3_NS_PER_SEC             1000000000ull
#define FSR3_FG_MAX_WAIT_NS         20000000ull
#define FSR3_FG_SLEEP_THRESHOLD_NS  200000ull
#define FSR3_FG_SPIN_MARGIN_NS      150000ull

/* Radix-inverse Halton sequence centered in (-0.5, +0.5) for index >= 1. */
static inline float extra_fsr3_halton(uint32_t index, uint32_t base) {
    float f = 1.0f, r = 0.0f;
    uint32_t i = index;
    while (i > 0) {
        f /= (float)base;
        r += f * (float)(i % base);
        i /= base;
    }
    return r - FSR3_HALTON_CENTER_OFFSET;
}

/* FSR 3.1 recommended Halton phase count: 8 * (out_w / render_w)^2, quantized to {16, 32, 64}. */
static inline uint32_t extra_fsr3_phase_count(uint32_t render_w, uint32_t out_w) {
    if (render_w == 0 || out_w <= render_w) return FSR3_PHASE_COUNT_MIN;
    float ratio = (float)out_w / (float)render_w;
    float raw = FSR3_BASE_PHASE_MULT * ratio * ratio;
    if (raw <= FSR3_PHASE_THRESH_MID) return FSR3_PHASE_COUNT_MIN;
    if (raw <= FSR3_PHASE_THRESH_MAX) return FSR3_PHASE_COUNT_MID;
    return FSR3_PHASE_COUNT_MAX;
}

/* Returns 1 only for 16:9 3D scene passes with depth testing enabled.
 * Rejects square shadow maps (e.g. 1024x1024), 2D UI/HUD passes (no depth),
 * and passes drawing directly to a presented swap target. */
static inline int extra_fsr3_should_jitter_pass(int has_depth, uint32_t rt_w, uint32_t rt_h,
                                                uint32_t depth_st0, int is_presented) {
    if (!has_depth || depth_st0 == 0 || is_presented) return 0;
    if (rt_w < FSR3_MIN_SCENE_WIDTH || rt_h < FSR3_MIN_SCENE_HEIGHT) return 0;
    float aspect = (float)rt_w / (float)rt_h;
    return (aspect >= FSR3_ASPECT_MIN_16_9 && aspect <= FSR3_ASPECT_MAX_16_9) ? 1 : 0;
}

/* Negative MIP LOD bias so 3D samplers select sharper mipmaps matching the output resolution. */
static inline float extra_fsr3_compute_mip_bias(float upscale_ratio, int jitter_active) {
    if (!jitter_active) return 0.0f;
    float r = upscale_ratio > 1.0f ? upscale_ratio : 1.0f;
    float bias = -log2f(r) - FSR3_MIP_BIAS_OFFSET;
    if (bias < FSR3_MIP_BIAS_MIN) bias = FSR3_MIP_BIAS_MIN;
    return bias;
}

/* Maps user sharpness [0.0 .. 1.0] to FSR 3.1 RCAS linear lobe scale. */
static inline float extra_fsr3_rcas_scale(float sharpness) {
    if (sharpness <= FSR3_RCAS_MIN_SHARPNESS) return 0.0f;
    if (sharpness > FSR3_RCAS_MAX_SHARPNESS) sharpness = FSR3_RCAS_MAX_SHARPNESS;
    float stops = (FSR3_RCAS_MAX_SHARPNESS - sharpness) * FSR3_RCAS_STOP_SCALE;
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
    return (s >= FSR3_RENDER_SCALE_MIN && s <= FSR3_RENDER_SCALE_MAX) ? s : FSR3_RENDER_SCALE_DEFAULT;
}

static inline float extra_fsr3_mip_bias(void) {
    if (!extra_fsr3_eff_jitter()) return 0.0f;
    return FSR3_MIP_BIAS_DEFAULT;
}

/* Advances the per-frame Halton(2,3) camera jitter state at the end/start of a game frame. */
static inline void extra_fsr3_frame_step(uint32_t render_w, uint32_t out_w) {
    g_extra_fsr3.prev_jitter_px[0] = g_extra_fsr3.jitter_px[0];
    g_extra_fsr3.prev_jitter_px[1] = g_extra_fsr3.jitter_px[1];
    g_extra_fsr3.upscale_ratio = (render_w > 0 && out_w > 0) ? ((float)out_w / (float)render_w) : FSR3_RENDER_SCALE_DEFAULT;
    g_extra_fsr3.jitter_active = extra_fsr3_eff_jitter();
    if (g_extra_fsr3.jitter_active) {
        uint32_t phases = extra_fsr3_phase_count(render_w, out_w);
        uint32_t idx = (g_extra_fsr3.phase_index % phases) + 1u;
        g_extra_fsr3.phase_index++;
        g_extra_fsr3.jitter_px[0] = extra_fsr3_halton(idx, FSR3_HALTON_BASE_X);
        g_extra_fsr3.jitter_px[1] = extra_fsr3_halton(idx, FSR3_HALTON_BASE_Y);
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
    uint64_t now = (uint64_t)ts.tv_sec * FSR3_NS_PER_SEC + (uint64_t)ts.tv_nsec;
    if (now >= target_ns) return;
    if (target_ns - now > FSR3_FG_MAX_WAIT_NS) target_ns = now + FSR3_FG_MAX_WAIT_NS; /* safety clamp: 20 ms */
    uint64_t rem_ns = target_ns - now;
    uint64_t t_start = now;
    if (rem_ns > FSR3_FG_SLEEP_THRESHOLD_NS) {
        uint64_t wake_ns = target_ns - FSR3_FG_SPIN_MARGIN_NS;
        struct timespec req = {(time_t)(wake_ns / FSR3_NS_PER_SEC), (long)(wake_ns % FSR3_NS_PER_SEC)};
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &req, NULL);
    }
    for (;;) {
        clock_gettime(CLOCK_MONOTONIC, &ts);
        uint64_t cur = (uint64_t)ts.tv_sec * FSR3_NS_PER_SEC + (uint64_t)ts.tv_nsec;
        if (cur >= target_ns) break;
        #if defined(__x86_64__) || defined(_M_X64)
        __builtin_ia32_pause();
        #else
        sched_yield();
        #endif
    }
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t end = (uint64_t)ts.tv_sec * FSR3_NS_PER_SEC + (uint64_t)ts.tv_nsec;
    g_blocked_ns += (end > t_start ? (end - t_start) : 0);
}

static inline void extra_fsr3_fg_pace_half(uint64_t busy_so_far_ns) {
    (void)busy_so_far_ns;
}
#endif /* FSR3_MATH_ONLY */

#ifdef __cplusplus
}
#endif
