#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "extra/toggles/ac_toggles.h"

#define EXTRA_VTX_MAX_COORD       50000.0f
#define EXTRA_LIGHT_MAX_INTENSITY 1000.0f
#define EXTRA_MAX_DIAG_WARNINGS   20

/* Cached environment toggles for vertex validation and guard. */
static inline int extra_vtx_debug_enabled(void) {
    if (g_extra_vtx_debug >= 0) return g_extra_vtx_debug; /* [extra] overlay override */
    static int cached = -2;
    if (cached == -2) {
        const char *e = getenv("LARECOMP_DEBUG_VTX");
        if (!e) e = getenv("AC_DEBUG_VTX");
        cached = (e && atoi(e) > 0) ? 1 : 0;
    }
    return cached;
}

static inline int extra_vtx_guard_enabled(void) {
    if (g_extra_vtx_guard >= 0) return g_extra_vtx_guard; /* [extra] overlay override */
    static int cached = -2;
    if (cached == -2) {
        const char *e = getenv("LARECOMP_GUARD_VTX");
        if (!e) e = getenv("AC_GUARD_VTX");
        cached = (e && atoi(e) > 0) ? 1 : 0;
    }
    return cached;
}

/* Validates a vertex position stream, catching NaN/Inf or coordinate spikes (vertex explosion).
 * When guard is active, clamps anomalous coordinates to origin to prevent visual explosion. */
static inline bool extra_vtx_check_stream(const void *ptr, uint32_t count, uint32_t stride, uint64_t shader) {
    if (!ptr || !count || stride < 12) return true;
    int guard = extra_vtx_guard_enabled();
    int debug = extra_vtx_debug_enabled();
    if (!guard && !debug) return true;

    const uint8_t *base = (const uint8_t *)ptr;
    static uint32_t warned = 0;
    bool clean = true;
    uint32_t samples = count < 128 ? count : 64;

    for (uint32_t i = 0; i < samples; i++) {
        float *pos = (float *)(base + (size_t)i * stride);
        for (int c = 0; c < 3; c++) {
            float v = pos[c];
            if (isnan(v) || isinf(v) || fabsf(v) > EXTRA_VTX_MAX_COORD) {
                clean = false;
                if (warned < EXTRA_MAX_DIAG_WARNINGS) {
                    warned++;
                    fprintf(stderr, "[vtx-guard] VERTEX EXPLOSION at idx %u coord %d: %.2f (shader=%016llx, stride=%u, count=%u)\n",
                            i, c, v, (unsigned long long)shader, stride, count);
                }
                if (guard) pos[c] = 0.0f;
            }
        }
    }
    return clean;
}

/* Validates lighting uniform constant buffer, logging aberrant multipliers that cause flash/blackout. */
static inline bool extra_light_check_cbuf(const void *ptr, size_t size, uint64_t shader) {
    if (!ptr || size < 16) return true;
    int guard = extra_vtx_guard_enabled();
    int debug = extra_vtx_debug_enabled();
    if (!guard && !debug) return true;

    const float *f = (const float *)ptr;
    size_t count = size / sizeof(float);
    if (count > 128) count = 128;
    static uint32_t warned = 0;

    for (size_t i = 0; i < count; i++) {
        float v = f[i];
        if (isnan(v) || isinf(v) || fabsf(v) > EXTRA_LIGHT_MAX_INTENSITY) {
            if (warned < EXTRA_MAX_DIAG_WARNINGS) {
                warned++;
                fprintf(stderr, "[light-guard] LIGHTING ANOMALY at offset %zu: %.2f (shader=%016llx, size=%zu)\n",
                        i * sizeof(float), v, (unsigned long long)shader, size);
            }
            return false;
        }
    }
    return true;
}

/* Vertex-stream guard; call in draw.c BEFORE the stream is copied/bound, otherwise
 * the clamp lands in guest RAM after the Vulkan copy was already made (copy path). */
static inline void extra_vtx_guard_stream(const void *ptr, uint32_t count, uint32_t stride, uint64_t shader) {
    if (ptr) extra_vtx_check_stream(ptr, count, stride, shader);
}

/* Lighting-constant-buffer hook; safe after stream binding: fragment cbufs are snapshotted
 * into Vulkan buffers later (stage_set during the draw call), so a clamp here still lands. */
static inline void extra_vtx_guard_cbuf(const void *fcb, size_t fcb_size, uint64_t shader) {
    if (fcb) extra_light_check_cbuf(fcb, fcb_size, shader);
}
