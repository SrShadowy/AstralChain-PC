#pragma once
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>
#include "hooks/sdl/gamepad_trampoline.h"
#include "hooks/ui/font_trampoline.h"

enum {
    EXTRA_XBOX_ATLAS_W = 256,
    EXTRA_XBOX_ATLAS_H = 512,
    EXTRA_XBOX_SS = 4,
    EXTRA_XBOX_RGBA_BPP = 4,
    EXTRA_XBOX_ALPHA_CH = 3,
    EXTRA_XBOX_HALO_PAD = 2,
    EXTRA_XBOX_FACE_SIZE = 22,
    EXTRA_XBOX_COVER_FULL = 255,
    EXTRA_XBOX_COVER_EMPTY = 0,
    EXTRA_XBOX_B_X0 = 125,
    EXTRA_XBOX_B_Y0 = 97,
    EXTRA_XBOX_A_X0 = 181,
    EXTRA_XBOX_A_Y0 = 97,
    EXTRA_XBOX_Y_X0 = 9,
    EXTRA_XBOX_Y_Y0 = 141,
    EXTRA_XBOX_X_X0 = 69,
    EXTRA_XBOX_X_Y0 = 141,
    EXTRA_XBOX_MINUS_X0 = 69,
    EXTRA_XBOX_MINUS_Y0 = 97,
    EXTRA_XBOX_MINUS_W = 22,
    EXTRA_XBOX_MINUS_H = 22,
    EXTRA_XBOX_PLUS_X0 = 69,
    EXTRA_XBOX_PLUS_Y0 = 185,
    EXTRA_XBOX_PLUS_W = 22,
    EXTRA_XBOX_PLUS_H = 22,
    EXTRA_XBOX_L_X0 = 68,
    EXTRA_XBOX_L_Y0 = 230,
    EXTRA_XBOX_L_W = 24,
    EXTRA_XBOX_L_H = 20,
    EXTRA_XBOX_R_X0 = 124,
    EXTRA_XBOX_R_Y0 = 142,
    EXTRA_XBOX_R_W = 24,
    EXTRA_XBOX_R_H = 20,
    EXTRA_XBOX_ZL_X0 = 180,
    EXTRA_XBOX_ZL_Y0 = 140,
    EXTRA_XBOX_ZL_W = 24,
    EXTRA_XBOX_ZL_H = 24,
    EXTRA_XBOX_ZR_X0 = 8,
    EXTRA_XBOX_ZR_Y0 = 184,
    EXTRA_XBOX_ZR_W = 24,
    EXTRA_XBOX_ZR_H = 24,
    EXTRA_XBOX_CIRCLE_HEAL_MIN = 4,
    EXTRA_XBOX_CIRCLE_HEAL_MAX = 17,
    EXTRA_XBOX_BUMPER_HEAL_X0 = 3,
    EXTRA_XBOX_BUMPER_HEAL_X1 = 20,
    EXTRA_XBOX_BUMPER_HEAL_Y0 = 3,
    EXTRA_XBOX_BUMPER_HEAL_Y1 = 16,
    EXTRA_XBOX_ZL_HEAL_X0 = 3,
    EXTRA_XBOX_ZL_HEAL_X1 = 20,
    EXTRA_XBOX_ZR_HEAL_X0 = 3,
    EXTRA_XBOX_ZR_HEAL_X1 = 20,
    EXTRA_XBOX_TRIG_HEAL_Y0 = 6,
    EXTRA_XBOX_TRIG_HEAL_Y1 = 19,
    EXTRA_XBOX_DIA_B_X0 = 118,
    EXTRA_XBOX_DIA_B_Y0 = 354,
    EXTRA_XBOX_DIA_A_X0 = 2,
    EXTRA_XBOX_DIA_A_Y0 = 398,
    EXTRA_XBOX_DIA_W = 38,
    EXTRA_XBOX_DIA_H = 37
};

#define EXTRA_XBOX_HALF 0.5f
#define EXTRA_XBOX_MENU_BAR_W_RATIO 0.48f
#define EXTRA_XBOX_MENU_BAR_H_RATIO 0.085f
#define EXTRA_XBOX_MENU_BAR_STEP_RATIO 0.17f
#define EXTRA_XBOX_VIEW_BOX_RATIO 0.28f
#define EXTRA_XBOX_VIEW_OFFSET_RATIO 0.09f
#define EXTRA_XBOX_VIEW_STROKE_RATIO 0.065f
#define EXTRA_XBOX_BUMPER_PAD_X_RATIO 0.14f
#define EXTRA_XBOX_BUMPER_PAD_Y_RATIO 0.16f
#define EXTRA_XBOX_TRIG_PAD_X_RATIO 0.16f
#define EXTRA_XBOX_TRIG_PAD_Y_RATIO 0.20f

static inline void extra_xbox_fill_rrect(
    int w, int h, uint8_t *c,
    float x0, float y0, float x1, float y1, float r, uint8_t v
) {
    int iy0 = (int)floorf(y0);
    int iy1 = (int)ceilf(y1);
    int ix0 = (int)floorf(x0);
    int ix1 = (int)ceilf(x1);
    for (int y = iy0; y < iy1; y++) {
        if (y < 0 || y >= h) {
            continue;
        }
        for (int x = ix0; x < ix1; x++) {
            if (x < 0 || x >= w) {
                continue;
            }
            float px = (float)x + EXTRA_XBOX_HALF;
            float py = (float)y + EXTRA_XBOX_HALF;
            float dx = 0.0f;
            float dy = 0.0f;
            if (px < x0 + r) {
                dx = x0 + r - px;
            } else if (px > x1 - r) {
                dx = px - (x1 - r);
            }
            if (py < y0 + r) {
                dy = y0 + r - py;
            } else if (py > y1 - r) {
                dy = py - (y1 - r);
            }
            if (dx * dx + dy * dy <= r * r) {
                c[y * w + x] = v;
            }
        }
    }
}

static inline void extra_xbox_swap_box(
    uint8_t *data, uint32_t row,
    int ax0, int ay0, int bx0, int by0, int bw, int bh
) {
    for (int y = 0; y < bh; y++) {
        uint8_t *pa = data + (size_t)(ay0 + y) * row + (size_t)ax0 * EXTRA_XBOX_RGBA_BPP;
        uint8_t *pb = data + (size_t)(by0 + y) * row + (size_t)bx0 * EXTRA_XBOX_RGBA_BPP;
        for (int x = 0; x < bw * EXTRA_XBOX_RGBA_BPP; x++) {
            uint8_t tmp = pa[x];
            pa[x] = pb[x];
            pb[x] = tmp;
        }
    }
}

static inline void extra_xbox_heal_and_apply(
    uint8_t *data, uint32_t row,
    int sx0, int sy0, int sw, int sh,
    int hx0, int hy0, int hx1, int hy1,
    const uint8_t *cv
) {
    int cw = sw * EXTRA_XBOX_SS;
    for (int y = hy0; y <= hy1; y++) {
        uint8_t *line = data + (size_t)(sy0 + y) * row + (size_t)sx0 * EXTRA_XBOX_RGBA_BPP;
        for (int x = hx0; x <= hx1; x++) {
            line[x * EXTRA_XBOX_RGBA_BPP + EXTRA_XBOX_ALPHA_CH] = EXTRA_XBOX_COVER_FULL;
        }
    }
    for (int y = 0; y < sh; y++) {
        uint8_t *line = data + (size_t)(sy0 + y) * row + (size_t)sx0 * EXTRA_XBOX_RGBA_BPP;
        for (int x = 0; x < sw; x++) {
            unsigned sum = 0;
            for (int j = 0; j < EXTRA_XBOX_SS; j++) {
                for (int i = 0; i < EXTRA_XBOX_SS; i++) {
                    sum += cv[(y * EXTRA_XBOX_SS + j) * cw + x * EXTRA_XBOX_SS + i];
                }
            }
            unsigned cov = sum / (EXTRA_XBOX_SS * EXTRA_XBOX_SS);
            uint8_t *a = &line[x * EXTRA_XBOX_RGBA_BPP + EXTRA_XBOX_ALPHA_CH];
            *a = (uint8_t)(((unsigned)(*a) * cov) / EXTRA_XBOX_COVER_FULL);
        }
    }
}

static inline void extra_xbox_cut_menu_icon(int w, int h, uint8_t *c) {
    float s = fminf((float)w, (float)h);
    float cx = (float)w * EXTRA_XBOX_HALF;
    float cy = (float)h * EXTRA_XBOX_HALF;
    float bw = s * EXTRA_XBOX_MENU_BAR_W_RATIO;
    float bh = s * EXTRA_XBOX_MENU_BAR_H_RATIO;
    float step = s * EXTRA_XBOX_MENU_BAR_STEP_RATIO;
    for (int i = -1; i <= 1; i++) {
        float by = cy + (float)i * step;
        extra_xbox_fill_rrect(
            w, h, c,
            cx - bw * EXTRA_XBOX_HALF, by - bh * EXTRA_XBOX_HALF,
            cx + bw * EXTRA_XBOX_HALF, by + bh * EXTRA_XBOX_HALF,
            bh * EXTRA_XBOX_HALF, EXTRA_XBOX_COVER_EMPTY
        );
    }
}

static inline void extra_xbox_cut_view_icon(int w, int h, uint8_t *c) {
    float s = fminf((float)w, (float)h);
    float cx = (float)w * EXTRA_XBOX_HALF;
    float cy = (float)h * EXTRA_XBOX_HALF;
    float box = s * EXTRA_XBOX_VIEW_BOX_RATIO;
    float off = s * EXTRA_XBOX_VIEW_OFFSET_RATIO;
    float st = s * EXTRA_XBOX_VIEW_STROKE_RATIO;
    float ax0 = cx - off - box * EXTRA_XBOX_HALF;
    float ay0 = cy - off - box * EXTRA_XBOX_HALF;
    float bx0 = cx + off - box * EXTRA_XBOX_HALF;
    float by0 = cy + off - box * EXTRA_XBOX_HALF;
    extra_xbox_fill_rrect(w, h, c, ax0, ay0, ax0 + box, ay0 + box, st, EXTRA_XBOX_COVER_EMPTY);
    extra_xbox_fill_rrect(
        w, h, c,
        ax0 + st, ay0 + st, ax0 + box - st, ay0 + box - st,
        0.0f, EXTRA_XBOX_COVER_FULL
    );
    extra_xbox_fill_rrect(w, h, c, bx0, by0, bx0 + box, by0 + box, st, EXTRA_XBOX_COVER_EMPTY);
    extra_xbox_fill_rrect(
        w, h, c,
        bx0 + st, by0 + st, bx0 + box - st, by0 + box - st,
        0.0f, EXTRA_XBOX_COVER_FULL
    );
}

static inline void extra_xbox_patch_button(
    uint8_t *data, uint32_t row,
    int sx0, int sy0, int sw, int sh,
    int hx0, int hy0, int hx1, int hy1,
    const wchar_t *label, float pad_x_ratio, float pad_y_ratio, int icon_kind
) {
    int cw = sw * EXTRA_XBOX_SS;
    int ch = sh * EXTRA_XBOX_SS;
    uint8_t *cv = (uint8_t *)malloc((size_t)cw * (size_t)ch);
    if (!cv) {
        return;
    }
    memset(cv, EXTRA_XBOX_COVER_FULL, (size_t)cw * (size_t)ch);
    if (icon_kind == 1) {
        extra_xbox_cut_view_icon(cw, ch, cv);
    } else if (icon_kind == 2) {
        extra_xbox_cut_menu_icon(cw, ch, cv);
    } else if (label) {
        float px = (float)cw * pad_x_ratio;
        float py = (float)ch * pad_y_ratio;
        extra_cut_text(cw, ch, cv, label, px, py, (float)cw - px, (float)ch - py);
    }
    extra_xbox_heal_and_apply(data, row, sx0, sy0, sw, sh, hx0, hy0, hx1, hy1, cv);
    free(cv);
}

static inline void extra_xbox_apply_atlas(uint8_t *data, uint32_t row) {
    int swap_span = EXTRA_XBOX_FACE_SIZE + EXTRA_XBOX_HALO_PAD * 2;
    if (extra_gamepad_confirm_a_enabled()) {
        extra_xbox_swap_box(
            data, row,
            EXTRA_XBOX_DIA_B_X0, EXTRA_XBOX_DIA_B_Y0,
            EXTRA_XBOX_DIA_A_X0, EXTRA_XBOX_DIA_A_Y0,
            EXTRA_XBOX_DIA_W, EXTRA_XBOX_DIA_H
        );
    } else {
        extra_xbox_swap_box(
            data, row,
            EXTRA_XBOX_B_X0 - EXTRA_XBOX_HALO_PAD, EXTRA_XBOX_B_Y0 - EXTRA_XBOX_HALO_PAD,
            EXTRA_XBOX_A_X0 - EXTRA_XBOX_HALO_PAD, EXTRA_XBOX_A_Y0 - EXTRA_XBOX_HALO_PAD,
            swap_span, swap_span
        );
    }
    extra_xbox_swap_box(
        data, row,
        EXTRA_XBOX_Y_X0 - EXTRA_XBOX_HALO_PAD, EXTRA_XBOX_Y_Y0 - EXTRA_XBOX_HALO_PAD,
        EXTRA_XBOX_X_X0 - EXTRA_XBOX_HALO_PAD, EXTRA_XBOX_X_Y0 - EXTRA_XBOX_HALO_PAD,
        swap_span, swap_span
    );
    extra_xbox_patch_button(
        data, row,
        EXTRA_XBOX_MINUS_X0, EXTRA_XBOX_MINUS_Y0, EXTRA_XBOX_MINUS_W, EXTRA_XBOX_MINUS_H,
        EXTRA_XBOX_CIRCLE_HEAL_MIN, EXTRA_XBOX_CIRCLE_HEAL_MIN,
        EXTRA_XBOX_CIRCLE_HEAL_MAX, EXTRA_XBOX_CIRCLE_HEAL_MAX,
        NULL, 0.0f, 0.0f, 1
    );
    extra_xbox_patch_button(
        data, row,
        EXTRA_XBOX_PLUS_X0, EXTRA_XBOX_PLUS_Y0, EXTRA_XBOX_PLUS_W, EXTRA_XBOX_PLUS_H,
        EXTRA_XBOX_CIRCLE_HEAL_MIN, EXTRA_XBOX_CIRCLE_HEAL_MIN,
        EXTRA_XBOX_CIRCLE_HEAL_MAX, EXTRA_XBOX_CIRCLE_HEAL_MAX,
        NULL, 0.0f, 0.0f, 2
    );
    extra_xbox_patch_button(
        data, row,
        EXTRA_XBOX_L_X0, EXTRA_XBOX_L_Y0, EXTRA_XBOX_L_W, EXTRA_XBOX_L_H,
        EXTRA_XBOX_BUMPER_HEAL_X0, EXTRA_XBOX_BUMPER_HEAL_Y0,
        EXTRA_XBOX_BUMPER_HEAL_X1, EXTRA_XBOX_BUMPER_HEAL_Y1,
        L"LB", EXTRA_XBOX_BUMPER_PAD_X_RATIO, EXTRA_XBOX_BUMPER_PAD_Y_RATIO, 0
    );
    extra_xbox_patch_button(
        data, row,
        EXTRA_XBOX_R_X0, EXTRA_XBOX_R_Y0, EXTRA_XBOX_R_W, EXTRA_XBOX_R_H,
        EXTRA_XBOX_BUMPER_HEAL_X0, EXTRA_XBOX_BUMPER_HEAL_Y0,
        EXTRA_XBOX_BUMPER_HEAL_X1, EXTRA_XBOX_BUMPER_HEAL_Y1,
        L"RB", EXTRA_XBOX_BUMPER_PAD_X_RATIO, EXTRA_XBOX_BUMPER_PAD_Y_RATIO, 0
    );
    extra_xbox_patch_button(
        data, row,
        EXTRA_XBOX_ZL_X0, EXTRA_XBOX_ZL_Y0, EXTRA_XBOX_ZL_W, EXTRA_XBOX_ZL_H,
        EXTRA_XBOX_ZL_HEAL_X0, EXTRA_XBOX_TRIG_HEAL_Y0,
        EXTRA_XBOX_ZL_HEAL_X1, EXTRA_XBOX_TRIG_HEAL_Y1,
        L"LT", EXTRA_XBOX_TRIG_PAD_X_RATIO, EXTRA_XBOX_TRIG_PAD_Y_RATIO, 0
    );
    extra_xbox_patch_button(
        data, row,
        EXTRA_XBOX_ZR_X0, EXTRA_XBOX_ZR_Y0, EXTRA_XBOX_ZR_W, EXTRA_XBOX_ZR_H,
        EXTRA_XBOX_ZR_HEAL_X0, EXTRA_XBOX_TRIG_HEAL_Y0,
        EXTRA_XBOX_ZR_HEAL_X1, EXTRA_XBOX_TRIG_HEAL_Y1,
        L"RT", EXTRA_XBOX_TRIG_PAD_X_RATIO, EXTRA_XBOX_TRIG_PAD_Y_RATIO, 0
    );
}
