// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] Nintendo Switch touch panel emulation for mouse pointer input.
#pragma once

#ifndef _WIN32
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SWITCH_TOUCH_WIDTH          1280
#define SWITCH_TOUCH_HEIGHT         720
#define SWITCH_TOUCH_DIAMETER       15
#define SWITCH_TOUCH_MAX_CONTACTS   16
#define SWITCH_TOUCH_CONTACT_STRIDE 40
#define AC_TOUCH_ASPECT             (16.0f / 9.0f)
#define EXTRA_DEFAULT_TOUCH_WIN_W   1600
#define EXTRA_DEFAULT_TOUCH_WIN_H   900

typedef struct {
    uint64_t delta_time;
    uint32_t attributes;
    uint32_t finger_id;
    int32_t x;
    int32_t y;
    int32_t diameter_x;
    int32_t diameter_y;
    int32_t angle;
    uint32_t reserved;
} ExtraTouchContact;

static inline void extra_mouse_menu_window_size(void *sdl_win, int *w, int *h) {
    int ww = 0;
    int hh = 0;
    if (sdl_win) {
        SDL_GetWindowSize((SDL_Window *)sdl_win, &ww, &hh);
    }
    if (ww <= 0 || hh <= 0) {
        const char *ew = getenv("AC_WIDTH");
        const char *eh = getenv("AC_HEIGHT");
        ww = (ew && atoi(ew) > 0) ? atoi(ew) : EXTRA_DEFAULT_TOUCH_WIN_W;
        hh = (eh && atoi(eh) > 0) ? atoi(eh) : EXTRA_DEFAULT_TOUCH_WIN_H;
    }
    *w = ww;
    *h = hh;
}

static inline void extra_mouse_menu_panel_point_coords(
    float cur_x,
    float cur_y,
    int win_w,
    int win_h,
    int32_t *px,
    int32_t *py
) {
    if (win_w <= 0 || win_h <= 0) {
        *px = 0;
        *py = 0;
        return;
    }
    float vw = (float)win_w;
    float vh = (float)win_h;
    float ox = 0.0f;
    float oy = 0.0f;
    if ((float)win_w > (float)win_h * AC_TOUCH_ASPECT) {
        vw = (float)win_h * AC_TOUCH_ASPECT;
        ox = ((float)win_w - vw) * 0.5f;
    } else {
        vh = (float)win_w / AC_TOUCH_ASPECT;
        oy = ((float)win_h - vh) * 0.5f;
    }
    float nx = (cur_x - ox) / vw;
    float ny = (cur_y - oy) / vh;
    if (nx < 0.0f) {
        nx = 0.0f;
    } else if (nx > 1.0f) {
        nx = 1.0f;
    }
    if (ny < 0.0f) {
        ny = 0.0f;
    } else if (ny > 1.0f) {
        ny = 1.0f;
    }
    *px = (int32_t)(nx * (float)SWITCH_TOUCH_WIDTH);
    *py = (int32_t)(ny * (float)SWITCH_TOUCH_HEIGHT);
}

#ifdef __cplusplus
}
#endif
#endif
