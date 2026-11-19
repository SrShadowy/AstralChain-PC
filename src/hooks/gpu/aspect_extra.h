#pragma once
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sched.h>
#include "additions/toggles/ac_toggles.h"

#define HORIZON_MAIN_MODULE_BASE        0x7000010000ull

#define AC_ASPECT_RATIO_16_9            (16.0f / 9.0f)
#define AC_ASPECT_RATIO_16_10           (16.0f / 10.0f)
#define AC_ASPECT_RATIO_21_9            (21.0f / 9.0f)
#define AC_ASPECT_RATIO_32_9            (32.0f / 9.0f)

#define AC_RES_GRID_STEP_W              32
#define AC_RES_GRID_STEP_H              18
#define AC_RES_MIN_OUTPUT_W             1280
#define AC_RES_MIN_OUTPUT_H             720
#define AC_RES_MAX_OUTPUT_W             3840
#define AC_RES_MAX_OUTPUT_H             2160
#define AC_RES_MIN_SCENE_W              800
#define AC_RES_DEFAULT_WIDTH            1600
#define AC_RES_DEFAULT_HEIGHT           900
#define AC_RES_DEFAULT_L1_WIDTH         1440
#define AC_RES_DEFAULT_L1_HEIGHT        810
#define AC_RES_DEFAULT_L2_WIDTH         1280
#define AC_RES_DEFAULT_L2_HEIGHT        810
#define AC_RES_POOL_BASE_BYTES          0x13600000u
#define AC_RES_POOL_FIXED_BYTES         (87ull << 20)
#define AC_RES_POOL_BYTES_PER_PX        110ull
#define AC_RES_1080P_PIXELS             (1920ull * 1080ull)
#define AC_RES_BUFROOT_BASE             0x0AD60000u
#define AC_RES_GBUF_BASE                0x08F00000u
#define AC_RES_GBUF_BYTES_PER_PX        8ull
#define AC_MIB_ALIGN_MASK               0xFFFFFull

/* Site offsets in main module (v1.0.1) for Onda B resolution, heaps, aspect, drawdist & HUD SVC 0xB3 */
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
#define AC_SVC_B3_SITE_BUFROOT          0x575A30ull
#define AC_SVC_B3_SITE_GBUF             0x575C94ull
#define AC_SVC_B3_SITE_MIP_BIAS         0x9ED178ull
#define AC_SVC_B3_SITE_CAMERA_ASPECT    0x9A30BCull
#define AC_SVC_B3_SITE_CAMERA_RECT      0x9A30C4ull
#define AC_SVC_B3_SITE_DR_DEF_0         0x551DF0ull
#define AC_SVC_B3_SITE_DR_DEF_1         0x551E0Cull
#define AC_SVC_B3_SITE_DR_DEF_2         0x551E28ull
#define AC_SVC_B3_SITE_DR_DEF_L         0x551E44ull
#define AC_SVC_B3_SITE_DR_MOD_0         0x55F0ECull
#define AC_SVC_B3_SITE_DR_MOD_1         0x55F134ull
#define AC_SVC_B3_SITE_DR_MOD_2         0x55F17Cull
#define AC_SVC_B3_SITE_DR_MOD_L         0x55F1C4ull
#define AC_SVC_B3_SITE_NOHUD_23         0x9177ECull
#define AC_SVC_B3_SITE_NOHUD_22         0x915EA0ull

#define AC_UI_ARCHIVES_PTR_OFF          0xDC18F0ull
#define AC_UI_INST_ARCHIVE_OFF          0xB4u
#define AC_UI_ARCHIVE_ENTRY_BYTES       0x24u

typedef struct {
    int32_t width;
    int32_t height;
    int32_t scene_w;
    int32_t scene_h;
    float aspect;
    bool is_custom;
    bool scene_custom;
    bool aspect_custom;
    uint32_t pool_size;
    uint32_t gbuf_extra;
} AcResolutionConfig;

static inline void extra_res_snap_grid_w(int w, int min_w, int max_w, int32_t *ow, int32_t *oh) {
    int units = (w + (AC_RES_GRID_STEP_W / 2)) / AC_RES_GRID_STEP_W;
    int min_u = min_w / AC_RES_GRID_STEP_W;
    int max_u = max_w / AC_RES_GRID_STEP_W;
    if (units < min_u) units = min_u;
    if (units > max_u) units = max_u;
    *ow = units * AC_RES_GRID_STEP_W;
    *oh = units * AC_RES_GRID_STEP_H;
}

static inline bool extra_res_snap_16_9(int w, int h, int min_w, int max_w, int32_t *ow, int32_t *oh) {
    if (w <= 0 || h <= 0) return false;
    if ((int64_t)w * 9 == (int64_t)h * 16) {
        extra_res_snap_grid_w(w, min_w, max_w, ow, oh);
        return true;
    }
    *ow = w;
    *oh = h;
    return true;
}

static inline uint32_t extra_res_calc_budget(int32_t w, int32_t h) {
    uint64_t px = (uint64_t)w * (uint64_t)h;
    if (px <= AC_RES_1080P_PIXELS) return AC_RES_POOL_BASE_BYTES;
    uint64_t est = ((AC_RES_POOL_FIXED_BYTES + px * AC_RES_POOL_BYTES_PER_PX) * 115ull) / 100ull;
    uint64_t aligned = (est + AC_MIB_ALIGN_MASK) & ~AC_MIB_ALIGN_MASK;
    return (uint32_t)aligned;
}

static inline uint32_t extra_res_calc_gbuf_extra(int32_t w, int32_t h) {
    uint64_t px = (uint64_t)w * (uint64_t)h;
    if (px <= AC_RES_1080P_PIXELS) return 0u;
    uint64_t extra = ((px - AC_RES_1080P_PIXELS) * AC_RES_GBUF_BYTES_PER_PX + AC_MIB_ALIGN_MASK) & ~AC_MIB_ALIGN_MASK;
    return (uint32_t)extra;
}

static inline bool extra_res_parse_aspect(const char *s, float *out_ratio, bool *out_custom) {
    if (!s || !*s || !strcmp(s, "auto")) return false;
    if (!strcmp(s, "16:9") || !strcmp(s, "16x9")) {
        *out_ratio = AC_ASPECT_RATIO_16_9;
        *out_custom = false;
        return true;
    }
    if (!strcmp(s, "16:10") || !strcmp(s, "16x10")) {
        *out_ratio = AC_ASPECT_RATIO_16_10;
        *out_custom = true;
        return true;
    }
    if (!strcmp(s, "21:9") || !strcmp(s, "21x9")) {
        *out_ratio = AC_ASPECT_RATIO_21_9;
        *out_custom = true;
        return true;
    }
    if (!strcmp(s, "32:9") || !strcmp(s, "32x9")) {
        *out_ratio = AC_ASPECT_RATIO_32_9;
        *out_custom = true;
        return true;
    }
    return false;
}

static inline void extra_res_pick_auto_output(int32_t *ow, int32_t *oh) {
    static const int32_t kPresets[][2] = {
        {1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}
    };
    int win_h = g_extra_win_h;
    if (win_h <= 0) {
        const char *eh = getenv("AC_HEIGHT");
        if (eh && *eh) win_h = atoi(eh);
    }
    if (win_h <= 0) {
        *ow = AC_RES_DEFAULT_WIDTH;
        *oh = AC_RES_DEFAULT_HEIGHT;
        return;
    }
    *ow = kPresets[0][0];
    *oh = kPresets[0][1];
    for (int i = 1; i < 5; i++) {
        if (kPresets[i][1] <= win_h) {
            *ow = kPresets[i][0];
            *oh = kPresets[i][1];
        }
    }
}

static inline void extra_aspect_query_config(AcResolutionConfig *cfg) {
    static AcResolutionConfig s_cached;
    static atomic_int s_init_state = 0;

    if (atomic_load_explicit(&s_init_state, memory_order_acquire) == 2) {
        *cfg = s_cached;
        return;
    }

    int expected = 0;
    if (!atomic_compare_exchange_strong(&s_init_state, &expected, 1)) {
        while (atomic_load_explicit(&s_init_state, memory_order_acquire) != 2) {
            sched_yield();
        }
        *cfg = s_cached;
        return;
    }

    ac_config_sync_init_hook();

    s_cached.width = AC_RES_DEFAULT_WIDTH;
    s_cached.height = AC_RES_DEFAULT_HEIGHT;
    s_cached.aspect = AC_ASPECT_RATIO_16_9;
    s_cached.is_custom = false;
    s_cached.aspect_custom = false;

    const char *out_env = getenv("AC_OUTPUT_RES");
    const char *res_env = getenv("AC_RESOLUTION");
    const char *raw_out = (out_env && *out_env) ? out_env
                        : ((res_env && *res_env) ? res_env
                        : (g_extra_output_res[0] ? g_extra_output_res : NULL));

    if (raw_out && *raw_out) {
        if (!strcmp(raw_out, "auto")) {
            extra_res_pick_auto_output(&s_cached.width, &s_cached.height);
            s_cached.is_custom = (s_cached.width != AC_RES_DEFAULT_WIDTH || s_cached.height != AC_RES_DEFAULT_HEIGHT);
        } else {
            int rw = 0, rh = 0;
            if (sscanf(raw_out, "%dx%d", &rw, &rh) == 2 && rw > 0 && rh > 0) {
                if ((int64_t)rw * 9 == (int64_t)rh * 16) {
                    extra_res_snap_16_9(rw, rh, AC_RES_MIN_OUTPUT_W, AC_RES_MAX_OUTPUT_W,
                                        &s_cached.width, &s_cached.height);
                } else {
                    s_cached.width = rw;
                    s_cached.height = rh;
                    s_cached.aspect = (float)rw / (float)rh;
                    s_cached.aspect_custom = true;
                }
                s_cached.is_custom = (s_cached.width != AC_RES_DEFAULT_WIDTH || s_cached.height != AC_RES_DEFAULT_HEIGHT);
            }
        }
    }

    const char *asp_env = getenv("AC_ASPECT");
    const char *raw_asp = (asp_env && *asp_env) ? asp_env : g_extra_aspect_str;
    if (!extra_res_parse_aspect(raw_asp, &s_cached.aspect, &s_cached.aspect_custom)) {
        if (!s_cached.aspect_custom && g_extra_win_w > 0 && g_extra_win_h > 0 &&
            (int64_t)g_extra_win_w * 9 != (int64_t)g_extra_win_h * 16) {
            float wa = (float)g_extra_win_w / (float)g_extra_win_h;
            if (fabsf(wa - AC_ASPECT_RATIO_16_9) > 0.03f) {
                s_cached.aspect = wa;
                s_cached.aspect_custom = true;
            }
        }
    }

    s_cached.pool_size = extra_res_calc_budget(s_cached.width, s_cached.height);
    s_cached.gbuf_extra = extra_res_calc_gbuf_extra(s_cached.width, s_cached.height);

    float rs = g_extra_render_scale;
    if (rs <= 0.0f) {
        const char *rsc = getenv("AC_RENDER_SCALE");
        rs = (rsc && *rsc) ? (float)atof(rsc) : 1.0f;
    }
    if (rs >= 0.50f && rs < 0.995f && (int64_t)s_cached.width * 9 == (int64_t)s_cached.height * 16) {
        int raw_sw = (int)lroundf((float)s_cached.width * rs);
        extra_res_snap_grid_w(raw_sw, AC_RES_MIN_SCENE_W, s_cached.width,
                              &s_cached.scene_w, &s_cached.scene_h);
        s_cached.scene_custom = true;
    } else {
        s_cached.scene_w = s_cached.width;
        s_cached.scene_h = s_cached.height;
        s_cached.scene_custom = s_cached.is_custom;
    }

    atomic_store_explicit(&s_init_state, 2, memory_order_release);
    *cfg = s_cached;
}

static inline uint64_t extra_res_output_extra_bytes(void) {
    AcResolutionConfig cfg;
    extra_aspect_query_config(&cfg);
    uint64_t pool_extra = (cfg.pool_size > AC_RES_POOL_BASE_BYTES)
                        ? (uint64_t)(cfg.pool_size - AC_RES_POOL_BASE_BYTES) : 0ull;
    return pool_extra + (uint64_t)cfg.gbuf_extra;
}

static inline float extra_draw_distance_factor(void) {
    int pct = g_extra_draw_distance;
    if (pct < EXTRA_DRAW_DISTANCE_MIN) {
        const char *e = getenv("AC_DRAW_DISTANCE");
        pct = (e && *e) ? atoi(e) : EXTRA_DRAW_DISTANCE_DEFAULT;
    }
    if (pct < EXTRA_DRAW_DISTANCE_MIN) pct = EXTRA_DRAW_DISTANCE_MIN;
    if (pct > EXTRA_DRAW_DISTANCE_MAX) pct = EXTRA_DRAW_DISTANCE_MAX;
    return (float)pct / 100.0f;
}

static inline bool extra_no_hud_active(void) {
    if (g_extra_no_hud >= 0) return g_extra_no_hud != 0;
    const char *e = getenv("AC_HUD");
    if (e && *e) return atoi(e) == 0;
    return false;
}

static inline bool extra_hud_area_centered(void) {
    if (g_extra_hud_area >= 0) return g_extra_hud_area == EXTRA_HUD_AREA_16_9;
    const char *e = getenv("AC_HUD_AREA");
    if (e && *e && !strcmp(e, "screen")) return false;
    return true;
}

static inline bool extra_aspect_is_widescreen(void) {
    AcResolutionConfig cfg;
    extra_aspect_query_config(&cfg);
    return cfg.aspect > (AC_ASPECT_RATIO_16_9 * 1.01f);
}
