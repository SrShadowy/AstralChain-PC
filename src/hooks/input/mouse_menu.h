#pragma once

#ifndef _WIN32
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pointer for UI mode (extra_mouse_mode_is_captured() == 0).
 *
 * What was measured (docs/plan-mouse-menu.md §4), not assumed:
 *
 *  - The game polls nn::hid::GetMouseState every frame (0x3e4120 -> 0x577000 ->
 *    0x9acd50 -> 0x9acad0), so the earlier "mouse is only read at boot" note was
 *    wrong. But the wrapper only builds an object at *(0xdc2540) and nothing in
 *    the image ever reads that object's fields: the sole non-writing accesses
 *    are the wrapper's own edge detection. The UI has no mouse hit-testing to
 *    feed, and the game never sends HID command 21, so MouseState stays zero.
 *  - A contact published on the touch panel drives the game's own touch state
 *    machine (idle->began->held->released, watched live at ctx+0x24). No menu
 *    reached in the boot sequence hit-tests the panel either: a contact on a
 *    DATA SLOT does not change the selection.
 *  - The d-pad translated from the pointer does work: one injected LEFT press
 *    moves LOAD DATA from slot B to slot A, at the same 0.06 s the pulse uses.
 *
 * So the pointer drives the pad, and the pad is what menus actually obey. The
 * selection cannot be aimed at absolute items, because the game exposes neither
 * layout nor selection state; the pointer maps to relative pad steps.
 *
 *   movement  -> d-pad steps every AC_MOUSE_MENU_CELL points travelled
 *   left      -> A (confirm)
 *   right     -> B (back)
 *   wheel     -> d-pad up / down
 *   optional  -> a real touch contact at the cursor (AC_MOUSE_MENU_TOUCH=1) */

#define AC_MOUSE_MENU_PULSE_TICKS 1152000ULL  /* ~0.06 s @ 19.2 MHz: one press */
#define AC_MOUSE_MENU_GAP_TICKS   960000ULL   /* ~0.05 s @ 19.2 MHz: release between presses */
#define AC_MOUSE_MENU_CELL_DEFAULT 40.0f
#define AC_MOUSE_MENU_CELL_MIN     10.0f
#define AC_MOUSE_MENU_CELL_MAX     200.0f
#define AC_MOUSE_MENU_QUEUE_MAX    4

/* Switch touch panel: nn::hid::TouchScreenState<16> contacts, panel pixels. */
#define SWITCH_TOUCH_WIDTH  1280
#define SWITCH_TOUCH_HEIGHT 720
#define SWITCH_TOUCH_DIAMETER 15
#define AC_TOUCH_ASPECT (16.0f / 9.0f) /* the game presents a 16:9 image */

/* SDL button numbers (extra_window_poll_events passes e.button.button). */
#define MOUSE_BTN_LEFT_CLICK  1
#define MOUSE_BTN_RIGHT_CLICK 3

#define SWITCH_BTN_BIT_A      0
#define SWITCH_BTN_BIT_B      1
#define SWITCH_BTN_BIT_DLEFT  12
#define SWITCH_BTN_BIT_DUP    13
#define SWITCH_BTN_BIT_DRIGHT 14
#define SWITCH_BTN_BIT_DDOWN  15

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

enum ExtraMouseMenuDir {
    EXTRA_MOUSE_MENU_DIR_UP    = 0,
    EXTRA_MOUSE_MENU_DIR_DOWN  = 1,
    EXTRA_MOUSE_MENU_DIR_LEFT  = 2,
    EXTRA_MOUSE_MENU_DIR_RIGHT = 3,
    EXTRA_MOUSE_MENU_DIR_COUNT = 4
};

static int g_mouse_menu_enabled = 1;
static int g_mouse_menu_motion_steps = 1; /* (movement drives the list: AC_MOUSE_MENU_MOTION=0 disables) */
static int g_mouse_menu_touch_mode = 0;
static int g_mouse_menu_config_loaded = 0;
static float g_mouse_menu_cell_size = AC_MOUSE_MENU_CELL_DEFAULT;
static float g_mouse_menu_accum_x = 0.0f;
static float g_mouse_menu_accum_y = 0.0f;

/* Cursor in window points, as SDL reports it. */
static float g_mouse_cursor_x = 0.0f;
static float g_mouse_cursor_y = 0.0f;
static int g_mouse_touch_held;
static uint64_t g_mouse_touch_until;
/* A warp (entering UI mode centres the pointer) reports one motion event with a
 * large delta: it must move the cursor, not the selection. */
static int g_mouse_menu_ignore_motion;

static uint64_t g_mouse_menu_btn_until[2] = {0, 0};  /* (A: click, B: right click) */
static uint64_t g_mouse_menu_btn_gap[2] = {0, 0};
static unsigned g_mouse_menu_btn_queue[2] = {0, 0};

static uint64_t g_mouse_menu_dir_until[EXTRA_MOUSE_MENU_DIR_COUNT] = {0};
static uint64_t g_mouse_menu_dir_gap[EXTRA_MOUSE_MENU_DIR_COUNT] = {0};
static unsigned g_mouse_menu_dir_queue[EXTRA_MOUSE_MENU_DIR_COUNT] = {0};

__attribute__((weak)) uint32_t extra_sync_read_menu_mouse(int *nav, int *motion, float *cell) {
    (void)nav;
    (void)motion;
    (void)cell;
    return 0;
}

static inline void extra_mouse_menu_sync(void) {
    static uint32_t seen_gen = 0;
    int nav = g_mouse_menu_enabled;
    int motion = g_mouse_menu_motion_steps;
    float cell = g_mouse_menu_cell_size;
    uint32_t gen = extra_sync_read_menu_mouse(&nav, &motion, &cell);
    if (gen && gen != seen_gen) {
        seen_gen = gen;
        g_mouse_menu_enabled = nav;
        g_mouse_menu_motion_steps = motion;
        g_mouse_menu_cell_size = cell;
    }
}

static inline void extra_mouse_menu_load_config(void) {
    if (!g_mouse_menu_config_loaded) {
        g_mouse_menu_config_loaded = 1;
        const char *e = getenv("AC_MOUSE_MENUS");
        if (e) g_mouse_menu_enabled = (*e != '0');
        e = getenv("AC_MOUSE_MENU_MOTION");
        if (e) g_mouse_menu_motion_steps = (*e != '0');
        e = getenv("AC_MOUSE_MENU_TOUCH");
        if (e) g_mouse_menu_touch_mode = (*e != '0');
        e = getenv("AC_MOUSE_MENU_CELL");
        if (e && *e) {
            float c = (float)atof(e);
            if (c >= AC_MOUSE_MENU_CELL_MIN && c <= AC_MOUSE_MENU_CELL_MAX) g_mouse_menu_cell_size = c;
        }
    }
    extra_mouse_menu_sync();
}

static inline int extra_mouse_menu_is_enabled(void) {
    extra_mouse_menu_load_config();
    return g_mouse_menu_enabled;
}

static inline void extra_mouse_menu_update_cursor(float x, float y) {
    g_mouse_cursor_x = x;
    g_mouse_cursor_y = y;
}

/* Called right after a programmatic warp: the next motion event only places the
 * cursor (it carries the whole jump as a delta). */
static inline void extra_mouse_menu_skip_motion(void) {
    g_mouse_menu_ignore_motion = 1;
}

/* Window size in points: the space SDL events use, so an event cursor maps
 * without a HiDPI conversion. */
static inline void extra_mouse_menu_window_size(void *sdl_win, int *w, int *h) {
    int ww = 0, hh = 0;
    if (sdl_win) SDL_GetWindowSize((SDL_Window*)sdl_win, &ww, &hh);
    if (ww <= 0 || hh <= 0) {
        const char *ew = getenv("AC_WIDTH"), *eh = getenv("AC_HEIGHT");
        ww = (ew && atoi(ew) > 0) ? atoi(ew) : 1600;
        hh = (eh && atoi(eh) > 0) ? atoi(eh) : 900;
    }
    *w = ww;
    *h = hh;
}

/* Window point -> touch panel point. The game presents a 16:9 image, so the
 * mapping is a centred 16:9 rectangle inside the window (letterbox on the odd
 * axis), with no knowledge of the game's render crop. */
#define SWITCH_TOUCH_MAX_CONTACTS 16
#define SWITCH_TOUCH_CONTACT_STRIDE 40

static inline void extra_mouse_menu_panel_point(int win_w, int win_h, int32_t *px, int32_t *py) {
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
    float nx = (g_mouse_cursor_x - ox) / vw;
    float ny = (g_mouse_cursor_y - oy) / vh;
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

/* A contact is down while the button is held and for AC_MOUSE_MENU_PULSE_TICKS
 * after it is released: a click shorter than one 60 Hz frame would otherwise be
 * published and cleared before the game read it. */
static inline int extra_mouse_menu_touch_active(uint64_t now) {
    extra_mouse_menu_load_config();
    if (!g_mouse_menu_touch_mode) return 0;
    if (!g_mouse_touch_held && g_mouse_touch_until && now >= g_mouse_touch_until) g_mouse_touch_until = 0;
    return g_mouse_touch_held || g_mouse_touch_until;
}

/* Fills one HidTouchSample (hid_service.h) with a single contact. */
static inline void extra_mouse_menu_fill_touch(void *touch_sample_ptr, int win_w, int win_h, uint64_t now) {
    if (!touch_sample_ptr) return;
    typedef struct {
        uint64_t sample;
        uint32_t count;
        uint32_t reserved;
        uint8_t touches[SWITCH_TOUCH_MAX_CONTACTS][SWITCH_TOUCH_CONTACT_STRIDE];
    } Sample;
    Sample *s = (Sample *)touch_sample_ptr;
    if (!extra_mouse_menu_touch_active(now)) {
        s->count = 0;
        return;
    }
    ExtraTouchContact contact;
    memset(&contact, 0, sizeof(contact));
    contact.attributes = 1; /* (connected) */
    extra_mouse_menu_panel_point(win_w, win_h, &contact.x, &contact.y);
    contact.diameter_x = SWITCH_TOUCH_DIAMETER;
    contact.diameter_y = SWITCH_TOUCH_DIAMETER;
    s->count = 1;
    memcpy(s->touches[0], &contact, sizeof(contact));
}

static inline int extra_mouse_menu_has_click(void) {
    return g_mouse_touch_held || g_mouse_menu_btn_until[0] || g_mouse_menu_btn_queue[0];
}

static inline void extra_mouse_menu_reset(void) {
    g_mouse_menu_accum_x = g_mouse_menu_accum_y = 0.0f;
    g_mouse_touch_held = 0;
    g_mouse_touch_until = 0;
    for (int i = 0; i < 2; i++) g_mouse_menu_btn_until[i] = g_mouse_menu_btn_gap[i] = g_mouse_menu_btn_queue[i] = 0;
    for (int i = 0; i < EXTRA_MOUSE_MENU_DIR_COUNT; i++)
        g_mouse_menu_dir_until[i] = g_mouse_menu_dir_gap[i] = g_mouse_menu_dir_queue[i] = 0;
}

static inline void extra_mouse_menu_queue_dir(int dir) {
    if (dir < 0 || dir >= EXTRA_MOUSE_MENU_DIR_COUNT) return;
    if (g_mouse_menu_dir_queue[dir] < AC_MOUSE_MENU_QUEUE_MAX) g_mouse_menu_dir_queue[dir]++;
}

/* Movement -> discrete d-pad steps, one per AC_MOUSE_MENU_CELL points travelled:
 * the list follows the mouse the way a wheel-driven list does, in any menu.
 * Absolute aim is not possible: §4.4 shows the game exposes neither item
 * geometry nor the selected index, so a cell->item mapping would be invented. */
static inline void extra_mouse_menu_on_motion(float dx, float dy) {
    extra_mouse_menu_load_config();
    if (g_mouse_menu_ignore_motion) {
        g_mouse_menu_ignore_motion = 0;
        return;
    }
    if (!g_mouse_menu_enabled || !g_mouse_menu_motion_steps) return;
    float cell = g_mouse_menu_cell_size;
    g_mouse_menu_accum_x += dx;
    g_mouse_menu_accum_y += dy;
    while (g_mouse_menu_accum_y >= cell) {
        extra_mouse_menu_queue_dir(EXTRA_MOUSE_MENU_DIR_DOWN);
        g_mouse_menu_accum_y -= cell;
    }
    while (g_mouse_menu_accum_y <= -cell) {
        extra_mouse_menu_queue_dir(EXTRA_MOUSE_MENU_DIR_UP);
        g_mouse_menu_accum_y += cell;
    }
    while (g_mouse_menu_accum_x >= cell) {
        extra_mouse_menu_queue_dir(EXTRA_MOUSE_MENU_DIR_RIGHT);
        g_mouse_menu_accum_x -= cell;
    }
    while (g_mouse_menu_accum_x <= -cell) {
        extra_mouse_menu_queue_dir(EXTRA_MOUSE_MENU_DIR_LEFT);
        g_mouse_menu_accum_x += cell;
    }
}

/* in_menu: the cursor is free. Outside it a click is a game button (ZL) and
 * must not queue a confirmation. */
static inline void extra_mouse_menu_on_button(uint8_t btn, int down, int in_menu, uint64_t now) {
    extra_mouse_menu_load_config();
    if (!g_mouse_menu_enabled || !in_menu) return;
    if (btn == MOUSE_BTN_LEFT_CLICK) {
        g_mouse_touch_held = down;
        if (down) {
            g_mouse_touch_until = now + AC_MOUSE_MENU_PULSE_TICKS;
            if (g_mouse_menu_btn_queue[0] < AC_MOUSE_MENU_QUEUE_MAX) g_mouse_menu_btn_queue[0]++;
        }
    } else if (btn == MOUSE_BTN_RIGHT_CLICK && down && g_mouse_menu_btn_queue[1] < AC_MOUSE_MENU_QUEUE_MAX) {
        g_mouse_menu_btn_queue[1]++;
    }
}

static inline void extra_mouse_menu_on_wheel(int y, int in_menu) {
    extra_mouse_menu_load_config();
    if (!g_mouse_menu_enabled || !in_menu || !y) return;
    extra_mouse_menu_queue_dir(y > 0 ? EXTRA_MOUSE_MENU_DIR_UP : EXTRA_MOUSE_MENU_DIR_DOWN);
}

#define AC_MOUSE_MENU_BTN_A_BIT (1ull << SWITCH_BTN_BIT_A)
#define AC_MOUSE_MENU_BTN_B_BIT (1ull << SWITCH_BTN_BIT_B)

/* One button tap: held AC_MOUSE_MENU_PULSE_TICKS, then AC_MOUSE_MENU_GAP_TICKS
 * before the next. The pad is read once a frame, so a shorter press is lost. */
static inline void extra_mouse_menu_apply_tap(int slot, uint64_t bit, uint64_t *buttons, uint64_t now) {
    if (g_mouse_menu_btn_until[slot]) {
        if (now < g_mouse_menu_btn_until[slot]) {
            *buttons |= bit;
            return;
        }
        g_mouse_menu_btn_until[slot] = 0;
        g_mouse_menu_btn_gap[slot] = now + AC_MOUSE_MENU_GAP_TICKS;
    }
    if (g_mouse_menu_btn_queue[slot] && now >= g_mouse_menu_btn_gap[slot]) {
        g_mouse_menu_btn_queue[slot]--;
        g_mouse_menu_btn_until[slot] = now + AC_MOUSE_MENU_PULSE_TICKS;
        *buttons |= bit;
    }
}

static inline int extra_mouse_menu_dirs_idle(void) {
    for (int i = 0; i < EXTRA_MOUSE_MENU_DIR_COUNT; i++)
        if (g_mouse_menu_dir_until[i] || g_mouse_menu_dir_queue[i]) return 0;
    return 1;
}

/* Adds the queued menu presses to the pad sample the game reads. */
static inline void extra_mouse_menu_apply(uint64_t *buttons, uint64_t now) {
    if (!buttons) return;
    extra_mouse_menu_load_config();
    if (!g_mouse_menu_enabled) return;
    if (g_mouse_menu_btn_queue[0] && !extra_mouse_menu_dirs_idle()) return; /* (confirm after the list settles) */
    extra_mouse_menu_apply_tap(0, AC_MOUSE_MENU_BTN_A_BIT, buttons, now);
    extra_mouse_menu_apply_tap(1, AC_MOUSE_MENU_BTN_B_BIT, buttons, now);
    for (int i = 0; i < EXTRA_MOUSE_MENU_DIR_COUNT; i++) {
        if (g_mouse_menu_dir_until[i] && now >= g_mouse_menu_dir_until[i]) {
            g_mouse_menu_dir_until[i] = 0;
            g_mouse_menu_dir_gap[i] = now + AC_MOUSE_MENU_GAP_TICKS;
        }
        if (!g_mouse_menu_dir_until[i] && g_mouse_menu_dir_queue[i] && now >= g_mouse_menu_dir_gap[i]) {
            g_mouse_menu_dir_queue[i]--;
            g_mouse_menu_dir_until[i] = now + AC_MOUSE_MENU_PULSE_TICKS;
        }
    }
    if (g_mouse_menu_dir_until[EXTRA_MOUSE_MENU_DIR_UP]) *buttons |= 1ull << SWITCH_BTN_BIT_DUP;
    if (g_mouse_menu_dir_until[EXTRA_MOUSE_MENU_DIR_DOWN]) *buttons |= 1ull << SWITCH_BTN_BIT_DDOWN;
    if (g_mouse_menu_dir_until[EXTRA_MOUSE_MENU_DIR_LEFT]) *buttons |= 1ull << SWITCH_BTN_BIT_DLEFT;
    if (g_mouse_menu_dir_until[EXTRA_MOUSE_MENU_DIR_RIGHT]) *buttons |= 1ull << SWITCH_BTN_BIT_DRIGHT;
}

#ifdef __cplusplus
}
#endif
#endif
