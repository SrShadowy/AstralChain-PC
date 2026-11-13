#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define HORIZON_MAIN_MODULE_BASE        0x7000010000ull

#define AC_ASPECT_RATIO_16_9            (16.0f / 9.0f)
#define AC_ASPECT_RATIO_16_10           (16.0f / 10.0f)
#define AC_ASPECT_RATIO_21_9            (21.0f / 9.0f)
#define AC_ASPECT_RATIO_32_9            (32.0f / 9.0f)

#define AC_RES_DEFAULT_WIDTH            1600
#define AC_RES_DEFAULT_HEIGHT           900
#define AC_RES_DEFAULT_L1_WIDTH         1440
#define AC_RES_DEFAULT_L1_HEIGHT        810
#define AC_RES_DEFAULT_L2_WIDTH         1280
#define AC_RES_DEFAULT_L2_HEIGHT        810
#define AC_RES_POOL_BASE_BYTES          0x13600000u

/* Site offsets in main module for resolution and aspect SVC 0xB3 */
#define AC_SVC_B3_SITE_DISPLAY_W        0x4328C0ull
#define AC_SVC_B3_SITE_DISPLAY_H        0x4328C8ull
#define AC_SVC_B3_SITE_STARTUP_W        0x40D5E4ull
#define AC_SVC_B3_SITE_STARTUP_H        0x40D5F0ull
#define AC_SVC_B3_SITE_RECREATE_W       0x40FB04ull
#define AC_SVC_B3_SITE_RECREATE_H       0x40FB14ull
#define AC_SVC_B3_SITE_POOL_SIZE        0x40E0B0ull
#define AC_SVC_B3_SITE_LEVEL_0          0x40F184ull
#define AC_SVC_B3_SITE_LEVEL_1          0x40F1A4ull
#define AC_SVC_B3_SITE_LEVEL_2          0x40F1D0ull
#define AC_SVC_B3_SITE_CAMERA_ASPECT    0x9A30BCull
#define AC_SVC_B3_SITE_CAMERA_RECT      0x9A30C4ull

typedef struct {
    int32_t width;
    int32_t height;
    float aspect;
    bool is_custom;
    uint32_t pool_size;
} AcResolutionConfig;

static inline void extra_aspect_query_config(AcResolutionConfig *cfg) {
    static AcResolutionConfig s_cached = {0, 0, 0.0f, false, 0};
    static int s_initialized = 0;

    if (s_initialized) {
        *cfg = s_cached;
        return;
    }

    s_cached.width = AC_RES_DEFAULT_WIDTH;
    s_cached.height = AC_RES_DEFAULT_HEIGHT;
    s_cached.aspect = AC_ASPECT_RATIO_16_9;
    s_cached.is_custom = false;
    s_cached.pool_size = AC_RES_POOL_BASE_BYTES;

    const char *res_env = getenv("AC_RESOLUTION");
    if (res_env && *res_env) {
        int w = 0;
        int h = 0;
        if (sscanf(res_env, "%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
            s_cached.width = w;
            s_cached.height = h;
            s_cached.aspect = (float)w / (float)h;
            s_cached.is_custom = true;
            uint64_t base_pixels = (uint64_t)AC_RES_DEFAULT_WIDTH * AC_RES_DEFAULT_HEIGHT;
            uint64_t cur_pixels = (uint64_t)w * h;
            if (cur_pixels > base_pixels) {
                s_cached.pool_size = (uint32_t)((uint64_t)AC_RES_POOL_BASE_BYTES * cur_pixels / base_pixels);
            }
        }
    }

    const char *asp_env = getenv("AC_ASPECT");
    if (asp_env && *asp_env) {
        if (!strcmp(asp_env, "16:9") || !strcmp(asp_env, "16x9")) {
            s_cached.aspect = AC_ASPECT_RATIO_16_9;
        } else if (!strcmp(asp_env, "16:10") || !strcmp(asp_env, "16x10")) {
            s_cached.aspect = AC_ASPECT_RATIO_16_10;
        } else if (!strcmp(asp_env, "21:9") || !strcmp(asp_env, "21x9")) {
            s_cached.aspect = AC_ASPECT_RATIO_21_9;
        } else if (!strcmp(asp_env, "32:9") || !strcmp(asp_env, "32x9")) {
            s_cached.aspect = AC_ASPECT_RATIO_32_9;
        }
    }

    s_initialized = 1;
    *cfg = s_cached;
}

static inline bool extra_aspect_is_widescreen(void) {
    AcResolutionConfig cfg;
    extra_aspect_query_config(&cfg);
    return cfg.aspect > (AC_ASPECT_RATIO_16_9 * 1.01f);
}
