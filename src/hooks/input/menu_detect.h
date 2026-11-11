#pragma once

#ifndef _WIN32
#include <stdint.h>
#include <stdlib.h>
#include "hooks/input/mouse_mode.h"

#ifdef __cplusplus
extern "C" {
#endif

/* When the cursor is free and when the camera has the mouse.
 *
 * The game exposes no "a menu is open" flag: the input update at main+0x9ac1d0
 * reads NpadState and TouchScreenState and nothing reflects UI state
 * (docs/plan-mouse-menu.md §4.2). The switch therefore follows the player:
 *
 *   1. The app starts with the cursor free, so the title screen is usable with
 *      the mouse alone (extra/input/mouse_menu.h).
 *   2. A movement or camera key (the stick bindings: WASD / FTHG by default,
 *      and never the d-pad bindings) means "I am playing" only when the pointer
 *      has been still for a moment: a player walking a list with WASD right
 *      after using the mouse keeps the cursor, while starting to play from a
 *      resting pointer takes the camera with no extra key.
 *   3. The pause key (the game's MINUS action, Esc by default) mirrors the
 *      pause menu, which that same key both opens and closes: press it and the
 *      cursor follows. F10 toggles at any time.
 *
 *   AC_MOUSE_AUTO_MENU=0     rule 2 off (pause key and F10 only)
 *   AC_MOUSE_MENU_TIMEOUT=1  a free cursor returns to the camera after 5 s
 *                            without pointer use (off: never hide a cursor the
 *                            player may still need) */

#define AC_MENU_DETECT_INACTIVITY_TICKS 96000000ULL /* 5.0 s @ 19.2 MHz */
#define AC_MENU_POINTER_IDLE_TICKS      23040000ULL /* 1.2 s @ 19.2 MHz */

static int g_menu_auto_enabled = 1;
static int g_menu_timeout_enabled = 0;
static int g_menu_detect_config_loaded = 0;
static int g_menu_pause_held = 0;
static uint64_t g_menu_detect_last_activity = 0;

static inline void extra_menu_detect_load_config(void) {
    if (g_menu_detect_config_loaded) return;
    g_menu_detect_config_loaded = 1;
    const char *e = getenv("AC_MOUSE_AUTO_MENU");
    if (e) g_menu_auto_enabled = (*e != '0');
    e = getenv("AC_MOUSE_MENU_TIMEOUT");
    if (e) g_menu_timeout_enabled = (*e != '0');
}

/* Pointer activity: any motion or click. */
static inline void extra_menu_detect_touch(uint64_t now) {
    g_menu_detect_last_activity = now;
}

/* is_pause: bound to MINUS. is_play: bound to a stick direction (movement or
 * camera), never to a d-pad direction. Auto-repeat is ignored for the toggle. */
static inline void extra_menu_detect_on_key(int is_pause, int is_play, int down, void *sdl_win, uint64_t now) {
    extra_menu_detect_load_config();
    if (is_pause) {
        if (!down) {
            g_menu_pause_held = 0;
            return;
        }
        if (g_menu_pause_held) return; /* (leading edge only) */
        g_menu_pause_held = 1;
        extra_mouse_mode_toggle(sdl_win);
        return;
    }
    if (!down || !is_play || !g_menu_auto_enabled || extra_mouse_mode_is_captured()) return;
    if (extra_mouse_menu_has_click()) return; /* (a click is still being delivered) */
    /* A player walking a list with WASD right after using the mouse keeps the
     * cursor; one starting to play from a resting pointer takes the camera. */
    if (g_menu_detect_last_activity && now > g_menu_detect_last_activity &&
        now - g_menu_detect_last_activity < AC_MENU_POINTER_IDLE_TICKS) return;
    extra_mouse_mode_set(sdl_win, EXTRA_MOUSE_MODE_CAPTURED);
}

static inline void extra_menu_detect_tick(uint64_t now, void *sdl_win) {
    extra_menu_detect_load_config();
    if (!g_menu_timeout_enabled || extra_mouse_mode_is_captured()) return;
    if (!g_menu_detect_last_activity) {
        g_menu_detect_last_activity = now;
        return;
    }
    if (now > g_menu_detect_last_activity &&
        now - g_menu_detect_last_activity >= AC_MENU_DETECT_INACTIVITY_TICKS) {
        g_menu_detect_last_activity = 0;
        extra_mouse_mode_set(sdl_win, EXTRA_MOUSE_MODE_CAPTURED);
    }
}

#ifdef __cplusplus
}
#endif
#endif
