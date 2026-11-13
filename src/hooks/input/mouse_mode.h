#pragma once

#ifndef _WIN32
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hooks/input/mouse_menu.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AC_MOUSE_REF_SPEED 1500.0
#define AC_MOUSE_STICK_MAX 32767
#define AC_MOUSE_DEADZONE_BASE 0.10
#define AC_MOUSE_CURVE_POWER_EXP 1.35
#define AC_MOUSE_CURVE_AGGRESSIVE_EXP 1.75
#define AC_MOUSE_CURVE_LINEAR_EXP 1.0
#define AC_MOUSE_SENS_DEFAULT 1.0
#define AC_MOUSE_YSCALE_DEFAULT 1.0
#define AC_MOUSE_SENS_PERCENT_DIV 100.0
#define AC_MOUSE_SENS_MIN 0.05
#define AC_MOUSE_SENS_MAX 40.0
#define AC_MOUSE_YSCALE_MIN 0.05
#define AC_MOUSE_YSCALE_MAX 10.0
#define AC_MOUSE_CURVE_MIN 0.2
#define AC_MOUSE_CURVE_MAX 5.0
#define AC_MOUSE_MIN_MAGNITUDE 1e-6
#define AC_MOUSE_STICK_MAG_MAX 1.0
#define AC_MOUSE_CURVE_EPSILON 0.01

enum ExtraMouseMode {
    EXTRA_MOUSE_MODE_RELEASED = 0,
    EXTRA_MOUSE_MODE_CAPTURED = 1,
};

/* The app starts with the cursor free so the title screen is clickable with no
 * key at all; a game action key (extra/input/menu_detect.h) or F10 takes the
 * mouse for the camera. */
static int g_mouse_mode_captured = EXTRA_MOUSE_MODE_RELEASED;
static double g_mouse_yscale = AC_MOUSE_YSCALE_DEFAULT;
static double g_mouse_curve_exp = AC_MOUSE_CURVE_POWER_EXP;
/* (set by extra_window_init and every mode change: the UI-mode timeout and the
 * window-focus handler both change modes without a window argument) */
static void *g_mouse_mode_window = NULL;

static inline int extra_mouse_mode_is_captured(void) {
    return g_mouse_mode_captured == EXTRA_MOUSE_MODE_CAPTURED;
}

/* The window is created after the first mode decision; keep it for the mode
 * changes that have no window argument (UI timeout, window focus). The app
 * starts with the cursor free, so the panel cursor also starts centred. */
static inline void extra_mouse_mode_warp_center(void *sdl_win);

static inline void extra_mouse_mode_attach(void *sdl_win) {
    if (!sdl_win) return;
    g_mouse_mode_window = sdl_win;
    if (extra_mouse_mode_is_captured()) return;
    SDL_ShowCursor();
    extra_mouse_mode_warp_center(sdl_win);
}

/* Window sizes in points (SDL_Cursor / SDL_Event coordinates), the same space
 * extra_mouse_menu_panel_point() maps to the touch panel. */
static inline void extra_mouse_mode_warp_center(void *sdl_win) {
    if (!sdl_win) return;
    int win_w = 0, win_h = 0;
    SDL_GetWindowSize((SDL_Window*)sdl_win, &win_w, &win_h);
    if (win_w > 0 && win_h > 0) {
        SDL_WarpMouseInWindow((SDL_Window*)sdl_win, (float)(win_w / 2), (float)(win_h / 2));
        extra_mouse_menu_update_cursor((float)(win_w / 2), (float)(win_h / 2));
        extra_mouse_menu_skip_motion();
    }
}

static inline void extra_mouse_mode_set(void *sdl_win, int captured) {
    int want = captured ? EXTRA_MOUSE_MODE_CAPTURED : EXTRA_MOUSE_MODE_RELEASED;
    if (sdl_win) g_mouse_mode_window = sdl_win;
    if (g_mouse_mode_captured == want) return;
    g_mouse_mode_captured = want;
    extra_mouse_menu_reset();
    if (!sdl_win) sdl_win = g_mouse_mode_window;
    if (sdl_win) {
        SDL_SetWindowRelativeMouseMode((SDL_Window*)sdl_win, want == EXTRA_MOUSE_MODE_CAPTURED);
        if (want == EXTRA_MOUSE_MODE_CAPTURED) {
            SDL_HideCursor();
        } else {
            SDL_ShowCursor();
            extra_mouse_mode_warp_center(sdl_win); /* (the pointer starts at the centre of the screen) */
        }
    }
    printf("[input] mouse mode: %s (F10 to toggle)\n",
           want == EXTRA_MOUSE_MODE_CAPTURED ? "captured (gameplay)" : "released (free cursor / menu)");
}

static inline void extra_mouse_mode_toggle(void *sdl_win) {
    extra_mouse_mode_set(sdl_win, extra_mouse_mode_is_captured() ? EXTRA_MOUSE_MODE_RELEASED
                                                                 : EXTRA_MOUSE_MODE_CAPTURED);
}

/* Losing focus gives the mouse back (the desktop needs the cursor). The mode the
 * player chose is remembered, so an explicit F10 "menu mode" survives alt-tab
 * while a captured camera is restored on return. */
static int g_mouse_mode_suspended;

static inline void extra_mouse_mode_suspend(void *sdl_win) {
    if (!extra_mouse_mode_is_captured()) return;
    g_mouse_mode_suspended = 1;
    extra_mouse_mode_set(sdl_win, EXTRA_MOUSE_MODE_RELEASED);
}

static inline void extra_mouse_mode_resume(void *sdl_win) {
    if (!g_mouse_mode_suspended) return;
    g_mouse_mode_suspended = 0;
    extra_mouse_mode_set(sdl_win, EXTRA_MOUSE_MODE_CAPTURED);
}

static inline void extra_mouse_load_config(double *sens, int *invert) {
    const char *e = getenv("AC_MOUSE_SENS");
    if (e && *e) {
        double s = atof(e) / AC_MOUSE_SENS_PERCENT_DIV;
        if (s >= AC_MOUSE_SENS_MIN && s <= AC_MOUSE_SENS_MAX) *sens = s;
    }
    e = getenv("AC_MOUSE_INVERT");
    if (e) *invert = (*e == '1');
    e = getenv("AC_MOUSE_YSCALE");
    if (e && *e) {
        double y = atof(e);
        if (y >= AC_MOUSE_YSCALE_MIN && y <= AC_MOUSE_YSCALE_MAX) g_mouse_yscale = y;
    }
    e = getenv("AC_MOUSE_CURVE");
    if (e && *e) {
        if (!strcmp(e, "linear") || !strcmp(e, "1")) g_mouse_curve_exp = AC_MOUSE_CURVE_LINEAR_EXP;
        else if (!strcmp(e, "power")) g_mouse_curve_exp = AC_MOUSE_CURVE_POWER_EXP;
        else if (!strcmp(e, "aggressive") || !strcmp(e, "2")) g_mouse_curve_exp = AC_MOUSE_CURVE_AGGRESSIVE_EXP;
        else {
            double c = atof(e);
            if (c >= AC_MOUSE_CURVE_MIN && c <= AC_MOUSE_CURVE_MAX) g_mouse_curve_exp = c;
        }
    }
}

__attribute__((weak)) uint32_t extra_sync_read_mouse(int *look, double *sens, double *yscale, double *curve_exp, int *invert) {
    (void)look;
    (void)sens;
    (void)yscale;
    (void)curve_exp;
    (void)invert;
    return 0;
}

static inline void extra_mouse_sync_runtime(int *look, double *sens, int *invert) {
    static uint32_t seen_gen = 0;
    int nlook = *look;
    double nsens = *sens;
    double nyscale = g_mouse_yscale;
    double ncurve = g_mouse_curve_exp;
    int ninvert = *invert;
    uint32_t gen = extra_sync_read_mouse(&nlook, &nsens, &nyscale, &ncurve, &ninvert);
    if (gen && gen != seen_gen) {
        seen_gen = gen;
        *look = nlook;
        *sens = nsens;
        g_mouse_yscale = nyscale;
        g_mouse_curve_exp = ncurve;
        *invert = ninvert;
    }
}

/* Speed (counts/s) -> right stick deflection. Mag 0..1 is curved with pow() and
 * offset by a floor so slow movement still clears the game's stick dead zone;
 * direction is preserved. AC_MOUSE_SENS=100 + AC_MOUSE_CURVE=linear reproduces
 * the pre-2026-11 behaviour except for the (now lower) 10% floor. */
static inline void extra_mouse_calculate_stick(double vel_x, double vel_y, double sens, int invert, int32_t *rx, int32_t *ry) {
    double x = vel_x * sens / AC_MOUSE_REF_SPEED;
    double y = -vel_y * sens * g_mouse_yscale / AC_MOUSE_REF_SPEED;
    if (invert) y = -y;
    double m = sqrt(x * x + y * y);
    if (m < AC_MOUSE_MIN_MAGNITUDE || *rx || *ry) return;
    double m_norm = m > AC_MOUSE_STICK_MAG_MAX ? AC_MOUSE_STICK_MAG_MAX : m;
    double curved = (fabs(g_mouse_curve_exp - AC_MOUSE_CURVE_LINEAR_EXP) < AC_MOUSE_CURVE_EPSILON)
                        ? m_norm : pow(m_norm, g_mouse_curve_exp);
    double out = AC_MOUSE_DEADZONE_BASE + (1.0 - AC_MOUSE_DEADZONE_BASE) * curved;
    *rx = (int32_t)(x / m * out * AC_MOUSE_STICK_MAX);
    *ry = (int32_t)(y / m * out * AC_MOUSE_STICK_MAX);
}

#ifdef __cplusplus
}
#endif
#endif
