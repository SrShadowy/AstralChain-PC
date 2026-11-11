/* Self-test for the menu pointer (extra/input/mouse_menu.h, menu_detect.h).
 *
 * The pointer logic is pure translation (cursor -> touch panel, movement ->
 * d-pad steps, click -> A/B taps) plus a small state machine, so it can be
 * checked without the game: this file fails the audit when a change breaks the
 * mapping or the pulse spacing the game needs to see each press.
 *
 *   ./extra/tools/audit.sh          (runs it when SDL3 headers are available)
 *   gcc -I. -o /tmp/t extra/tools/mouse_pointer_test.c $(pkg-config --cflags --libs sdl3)
 */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include "hooks/input/mouse_mode.h"
#include "hooks/input/mouse_menu.h"
#include "hooks/input/menu_detect.h"

static uint64_t t_now = 0;
static uint64_t tick(uint64_t ns) { t_now += ns; return t_now; }

#define CHECK(cond, name)                                                              \
    do {                                                                               \
        if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
                       return 1; }                                                     \
    } while (0)

static uint64_t pads(uint64_t now) { uint64_t b = 0; extra_mouse_menu_apply(&b, now); return b; }

int main(void) {
    int w = 0, h = 0;
    int32_t px = 0, py = 0;

    /* Cursor -> panel: the window centre is the panel centre, corners clamp. */
    extra_mouse_menu_window_size(NULL, &w, &h);
    CHECK(w > 0 && h > 0, "window size fallback");
    extra_mouse_menu_update_cursor(800.0f, 450.0f);
    extra_mouse_menu_panel_point(1600, 900, &px, &py);
    CHECK(px == 640 && py == 360, "centred cursor maps to panel centre");
    extra_mouse_menu_update_cursor(0.0f, 0.0f);
    extra_mouse_menu_panel_point(1600, 900, &px, &py);
    CHECK(px == 0 && py == 0, "top-left maps to panel origin");
    extra_mouse_menu_update_cursor(1600.0f, 900.0f);
    extra_mouse_menu_panel_point(1600, 900, &px, &py);
    CHECK(px == 1280 && py == 720, "bottom-right maps to panel far corner");

    /* 21:9 window: the image is a centred 16:9 box, letterbox clamps. */
    extra_mouse_menu_update_cursor(320.0f, 540.0f);
    extra_mouse_menu_panel_point(2560, 1080, &px, &py);
    CHECK(px == 0 && py == 360, "letterbox left edge is panel x=0");
    extra_mouse_menu_update_cursor(0.0f, 540.0f);
    extra_mouse_menu_panel_point(2560, 1080, &px, &py);
    CHECK(px == 0 && py == 360, "cursor in the letterbox clamps to the image");

    /* Movement -> one d-pad step per cell, not before. */
    extra_mouse_menu_reset();
    extra_mouse_menu_on_motion(39.0f, 0.0f);
    CHECK(pads(tick(1000000)) == 0, "no step under one cell");
    extra_mouse_menu_on_motion(1.0f, 0.0f);
    CHECK(pads(tick(1000000)) == (1ull << 14), "40 points -> d-pad right");
    CHECK(pads(tick(576000)) == (1ull << 14), "press held ~60 ms");
    CHECK(pads(tick(1152000)) == 0, "press released");

    /* Regressão do que foi medido (docs/plan-mouse-menu.md §4.4): o jogo não
     * expõe geometria de item nem o índice selecionado, e nenhum menu testado
     * faz hit-test do painel. A posição do cursor sozinha, portanto, não pode
     * gerar passos: só o deslocamento gera. Guarda contra reintroduzir uma
     * "seleção posicional" que andaria por células inventadas (e oscilaria em
     * menus de 2 itens, onde 45 passos voltam ao item de origem). */
    extra_mouse_menu_reset();
    extra_mouse_menu_update_cursor(1500.0f, 800.0f);
    CHECK(pads(tick(1000000)) == 0, "cursor position alone queues no step");
    extra_mouse_menu_update_cursor(20.0f, 30.0f);
    CHECK(pads(tick(1000000)) == 0, "jumping the cursor queues no step");

    /* Click -> exactly one A tap, even if the click and release are one frame. */
    extra_mouse_menu_reset();
    extra_mouse_menu_on_button(MOUSE_BTN_LEFT_CLICK, 1, 1, t_now);
    extra_mouse_menu_on_button(MOUSE_BTN_LEFT_CLICK, 0, 1, t_now);
    CHECK(pads(tick(1)) == 1ull, "click -> A");
    CHECK(pads(tick(1152000)) == 0, "A released");
    CHECK(pads(tick(20000000)) == 0, "one press per click");

    /* Right click -> B. */
    extra_mouse_menu_reset();
    extra_mouse_menu_on_button(MOUSE_BTN_RIGHT_CLICK, 1, 1, t_now);
    CHECK(pads(tick(1)) == (1ull << 1), "right click -> B");

    /* Wheel -> d-pad up / down. */
    extra_mouse_menu_reset();
    extra_mouse_menu_on_wheel(1, 1);
    CHECK(pads(tick(1)) == (1ull << 13), "wheel up -> d-pad up");
    extra_mouse_menu_reset();
    extra_mouse_menu_on_wheel(-1, 1);
    CHECK(pads(tick(1)) == (1ull << 15), "wheel down -> d-pad down");

    /* A click is not a game button: nothing is queued outside menu mode. */
    extra_mouse_menu_reset();
    extra_mouse_menu_on_button(MOUSE_BTN_LEFT_CLICK, 1, 0, t_now);
    CHECK(pads(tick(1)) == 0, "click outside menu mode queues nothing");

    /* AC_MOUSE_MENUS=0: no pad injection at all. */
    setenv("AC_MOUSE_MENUS", "0", 1);
    g_mouse_menu_config_loaded = 0; g_mouse_menu_enabled = 1;
    extra_mouse_menu_reset();
    extra_mouse_menu_on_motion(500.0f, 500.0f);
    extra_mouse_menu_on_button(MOUSE_BTN_LEFT_CLICK, 1, 1, t_now);
    CHECK(pads(tick(1)) == 0, "AC_MOUSE_MENUS=0 injects nothing");
    CHECK(!extra_mouse_menu_is_enabled(), "AC_MOUSE_MENUS=0 reports disabled");
    setenv("AC_MOUSE_MENUS", "1", 1);
    g_mouse_menu_config_loaded = 0; g_mouse_menu_enabled = 1;

    /* Mode policy: free cursor at start, stick key takes it, pause toggles. */
    CHECK(!extra_mouse_mode_is_captured(), "app starts with a free cursor");
    g_menu_detect_last_activity = 0;
    extra_menu_detect_on_key(0, 1, 1, NULL, tick(1000000000));
    CHECK(extra_mouse_mode_is_captured(), "stick key takes the camera");
    extra_menu_detect_on_key(1, 0, 1, NULL, t_now);
    CHECK(!extra_mouse_mode_is_captured(), "pause key frees the cursor");
    extra_menu_detect_on_key(1, 0, 1, NULL, t_now);
    CHECK(!extra_mouse_mode_is_captured(), "auto-repeat is ignored while held");
    extra_menu_detect_on_key(1, 0, 0, NULL, t_now);
    extra_menu_detect_on_key(1, 0, 1, NULL, t_now);
    CHECK(extra_mouse_mode_is_captured(), "next pause press toggles back");
    extra_menu_detect_on_key(1, 0, 0, NULL, t_now);
    extra_menu_detect_on_key(1, 0, 1, NULL, t_now);
    CHECK(!extra_mouse_mode_is_captured(), "and frees it again");

    /* A key pressed while the pointer is active does not steal the cursor. */
    g_menu_detect_last_activity = t_now;
    extra_menu_detect_on_key(0, 1, 1, NULL, t_now + 1000000);
    CHECK(!extra_mouse_mode_is_captured(), "recent pointer use keeps the cursor");
    extra_menu_detect_on_key(0, 1, 1, NULL, t_now + AC_MENU_POINTER_IDLE_TICKS + 1);
    CHECK(extra_mouse_mode_is_captured(), "idle pointer lets the key take it");

    /* AC_MOUSE_AUTO_MENU=0: only pause / F10 change the mode. */
    extra_mouse_mode_set(NULL, 0);
    setenv("AC_MOUSE_AUTO_MENU", "0", 1);
    g_menu_detect_config_loaded = 0; g_menu_auto_enabled = 1;
    g_menu_detect_last_activity = 0;
    extra_menu_detect_on_key(0, 1, 1, NULL, t_now);
    CHECK(!extra_mouse_mode_is_captured(), "AC_MOUSE_AUTO_MENU=0 keeps the cursor");

    /* Opt-in touch contact: only with AC_MOUSE_MENU_TOUCH=1. */
    setenv("AC_MOUSE_MENU_TOUCH", "0", 1);
    g_mouse_menu_config_loaded = 0;
    extra_mouse_menu_reset();
    extra_mouse_menu_on_button(MOUSE_BTN_LEFT_CLICK, 1, 1, t_now);
    CHECK(!extra_mouse_menu_touch_active(t_now), "touch off by default");
    setenv("AC_MOUSE_MENU_TOUCH", "1", 1);
    g_mouse_menu_config_loaded = 0;
    extra_mouse_menu_on_button(MOUSE_BTN_LEFT_CLICK, 1, 1, t_now);
    CHECK(extra_mouse_menu_touch_active(t_now), "touch contact while held");
    extra_mouse_menu_on_button(MOUSE_BTN_LEFT_CLICK, 0, 1, t_now);
    CHECK(extra_mouse_menu_touch_active(t_now + 1), "touch latched past a short click");
    CHECK(!extra_mouse_menu_touch_active(t_now + AC_MOUSE_MENU_PULSE_TICKS),
          "touch released after the latch");

    printf("PASSED: mouse pointer self-test (mapping, pulses, mode policy)\n");
    return 0;
}
