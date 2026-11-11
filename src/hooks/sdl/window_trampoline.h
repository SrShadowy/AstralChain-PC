#pragma once
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "hooks/platform/platform_posix.h"
#include "hooks/input/mouse_mode.h"
#include "hooks/sdl/win32_window_types.h"
#include "hooks/sdl/gamepad_trampoline.h"
#include "additions/overlay/ac_overlay.h"
#include "additions/overlay/ac_reinfo.h"

#define EXTRA_DEFAULT_WIN_WIDTH 1600
#define EXTRA_DEFAULT_WIN_HEIGHT 900
#define EXTRA_EXIT_WATCHDOG_US 400000
#define EXTRA_APP_MSG_EXIT 4
#define EXTRA_APP_MSG_FOCUS 15
#define EXTRA_MOUSE_ACTIVITY_REL_SQ 2.0f
#define EXTRA_VK_MAX_ASCII_KEY 256

__attribute__((weak)) SDL_Window *g_sdl_window;
__attribute__((weak)) int g_sdl_input_focus;
__attribute__((weak)) int g_sdl_fullscreen;
__attribute__((weak)) int g_sdl_exit_requested;

static inline void *extra_window_ptr(void);

static inline void extra_window_get_size_hwnd(void *win_ptr, int *w, int *h) {
    SDL_Window *win = (SDL_Window *)win_ptr;
    if (!win) {
        win = g_sdl_window;
    }
    if (!win) {
        win = (SDL_Window *)extra_window_ptr();
    }
    int rw = 0;
    int rh = 0;
    if (win) {
        SDL_GetWindowSizeInPixels(win, &rw, &rh);
    }
    if (rw <= 0 || rh <= 0) {
        const char *ew = getenv("AC_WIDTH");
        const char *eh = getenv("AC_HEIGHT");
        rw = (ew && atoi(ew) > 0) ? atoi(ew) : EXTRA_DEFAULT_WIN_WIDTH;
        rh = (eh && atoi(eh) > 0) ? atoi(eh) : EXTRA_DEFAULT_WIN_HEIGHT;
    }
    if (w) {
        *w = rw;
    }
    if (h) {
        *h = rh;
    }
}

static inline void extra_window_get_size(int *w, int *h) {
    extra_window_get_size_hwnd(NULL, w, h);
}

static inline BOOL extra_set_window_pos(HWND w, HWND after, int x, int y, int cx, int cy, UINT flags) {
    (void)after;
    (void)x;
    (void)y;
    SDL_Window *win = (SDL_Window *)w;
    if (!win) {
        win = g_sdl_window;
    }
    if (win && cx > 0 && cy > 0 && !(flags & SWP_NOSIZE)) {
        SDL_SetWindowSize(win, cx, cy);
    }
    return 1;
}

static inline BOOL extra_show_window(HWND w, int s) {
    SDL_Window *win = (SDL_Window *)w;
    if (!win) {
        win = g_sdl_window;
    }
    if (win) {
        if (s == SW_HIDE) {
            SDL_HideWindow(win);
        } else {
            SDL_ShowWindow(win);
            SDL_RaiseWindow(win);
        }
    }
    return 1;
}

static inline uint8_t extra_sdl_to_vk(SDL_Keycode key) {
    if (key >= SDLK_A && key <= SDLK_Z) {
        return (uint8_t)('A' + (key - SDLK_A));
    }
    if (key >= SDLK_0 && key <= SDLK_9) {
        return (uint8_t)('0' + (key - SDLK_0));
    }
    switch (key) {
        case SDLK_RETURN:
            return VK_RETURN;
        case SDLK_ESCAPE:
            return VK_ESCAPE;
        case SDLK_BACKSPACE:
            return VK_BACK;
        case SDLK_TAB:
            return VK_TAB;
        case SDLK_SPACE:
            return VK_SPACE;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
            return VK_SHIFT;
        case SDLK_LCTRL:
        case SDLK_RCTRL:
            return VK_CONTROL;
        case SDLK_LALT:
        case SDLK_RALT:
            return VK_MENU;
        case SDLK_LEFT:
            return VK_LEFT;
        case SDLK_UP:
            return VK_UP;
        case SDLK_RIGHT:
            return VK_RIGHT;
        case SDLK_DOWN:
            return VK_DOWN;
        case SDLK_F11:
            return VK_F11;
        default:
            return key < EXTRA_VK_MAX_ASCII_KEY ? (uint8_t)key : 0;
    }
}

static inline void *extra_window_init(int w, int h, const char *title) {
    if (g_sdl_window) {
        return g_sdl_window;
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return NULL;
    }
    extra_open_initial_gamepad();
    const char *ew = getenv("AC_WIDTH");
    const char *eh = getenv("AC_HEIGHT");
    if (ew && atoi(ew) > 0) {
        w = atoi(ew);
    }
    if (eh && atoi(eh) > 0) {
        h = atoi(eh);
    }
    if (w <= 0) {
        w = EXTRA_DEFAULT_WIN_WIDTH;
    }
    if (h <= 0) {
        h = EXTRA_DEFAULT_WIN_HEIGHT;
    }

    g_sdl_window = SDL_CreateWindow(title ? title : "Astral Chain", w, h, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!g_sdl_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return NULL;
    }
    extra_mouse_mode_attach(g_sdl_window);
    ac_overlay_set_window(g_sdl_window);
    SDL_ShowWindow(g_sdl_window);
    SDL_RaiseWindow(g_sdl_window);
    /* The window is created and raised: input is live from here (SDL's focus
     * event may never arrive in an unattended run). */ // [extra]
    g_sdl_input_focus = 1; // [extra]

    const char *fs = getenv("AC_FULLSCREEN");
    if (fs && fs[0] == '1') {
        SDL_SetWindowFullscreen(g_sdl_window, true);
        g_sdl_fullscreen = 1;
    }
    printf("[video] SDL3 window initialized: %dx%d (fullscreen=%d)\n", w, h, g_sdl_fullscreen);
    return g_sdl_window;
}

static inline void *extra_window_ptr(void) {
    if (!g_sdl_window) {
        extra_window_init(EXTRA_DEFAULT_WIN_WIDTH, EXTRA_DEFAULT_WIN_HEIGHT, "Astral Chain");
    }
    return g_sdl_window;
}

static inline void extra_window_set_fullscreen(int on) {
    if (!g_sdl_window) {
        return;
    }
    g_sdl_fullscreen = on;
    SDL_SetWindowFullscreen(g_sdl_window, on ? true : false);
}

static void (*g_app_enqueue_fn)(unsigned) = NULL;

static void *extra_exit_watchdog_thread(void *unused) {
    (void)unused;
    usleep(EXTRA_EXIT_WATCHDOG_US);
    fflush(stdout);
    fflush(stderr);
    _exit(0);
    return NULL;
}

static inline void extra_trigger_exit(void) {
    if (g_sdl_exit_requested) {
        return;
    }
    g_sdl_exit_requested = 1;
    if (g_sdl_window) {
        SDL_HideWindow(g_sdl_window);
    }
    if (g_app_enqueue_fn) {
        g_app_enqueue_fn(EXTRA_APP_MSG_EXIT);
    }
    pthread_t th;
    pthread_create(&th, NULL, extra_exit_watchdog_thread, NULL);
    pthread_detach(th);
}

static inline int extra_window_poll_events(
    void (*key_cb)(uint8_t vk, int down),
    void (*mouse_motion_cb)(float x, float y, float dx, float dy),
    void (*mouse_button_cb)(uint8_t btn, int down),
    void (*mouse_wheel_cb)(int y)
) {
    if (!g_sdl_window) {
        return 0;
    }
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        ac_overlay_process_event(&e);
        int menu = ac_overlay_visible() || ac_reinfo_visible();
        switch (e.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                extra_trigger_exit();
                break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                g_sdl_input_focus = 1;
                extra_mouse_mode_resume(g_sdl_window);
                break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                g_sdl_input_focus = 0;
                extra_mouse_mode_suspend(g_sdl_window);
                break;
            case SDL_EVENT_KEY_DOWN:
                extra_set_active_device(EXTRA_DEVICE_KBM);
                if ((e.key.key == SDLK_INSERT || e.key.key == SDLK_F8) && !e.key.repeat) {
                    if (e.key.key == SDLK_INSERT) {
                        ac_overlay_toggle();
                    } else {
                        ac_reinfo_toggle();
                    }
                    if (ac_overlay_visible() || ac_reinfo_visible()) {
                        extra_mouse_mode_suspend(g_sdl_window);
                    } else if (g_sdl_input_focus) {
                        extra_mouse_mode_resume(g_sdl_window);
                    }
                } else if (e.key.key == SDLK_F11 && !e.key.repeat) {
                    extra_window_set_fullscreen(!g_sdl_fullscreen);
                } else if (e.key.key == SDLK_F10 && !e.key.repeat) {
                    extra_mouse_mode_toggle(g_sdl_window);
                } else if ((e.key.key == SDLK_F4 && (e.key.mod & SDL_KMOD_ALT)) ||
                           (e.key.key == SDLK_Q && (e.key.mod & SDL_KMOD_CTRL))) {
                    extra_trigger_exit();
                } else if (key_cb && !menu) {
                    key_cb(extra_sdl_to_vk(e.key.key), 1);
                }
                break;
            case SDL_EVENT_KEY_UP:
                if (key_cb && !menu && e.key.key != SDLK_F11 && e.key.key != SDLK_F10 &&
                    e.key.key != SDLK_INSERT && e.key.key != SDLK_F8) {
                    key_cb(extra_sdl_to_vk(e.key.key), 0);
                }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (menu) {
                    break;
                }
                if (e.motion.xrel * e.motion.xrel + e.motion.yrel * e.motion.yrel > EXTRA_MOUSE_ACTIVITY_REL_SQ) {
                    extra_set_active_device(EXTRA_DEVICE_KBM);
                }
                if (mouse_motion_cb) {
                    mouse_motion_cb(e.motion.x, e.motion.y, e.motion.xrel, e.motion.yrel);
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (menu) {
                    break;
                }
                extra_set_active_device(EXTRA_DEVICE_KBM);
                if (!extra_mouse_mode_is_captured()) {
                    extra_mouse_menu_update_cursor(e.button.x, e.button.y);
                }
                if (mouse_button_cb) {
                    mouse_button_cb((uint8_t)e.button.button, 1);
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (menu) {
                    break;
                }
                if (!extra_mouse_mode_is_captured()) {
                    extra_mouse_menu_update_cursor(e.button.x, e.button.y);
                }
                if (mouse_button_cb) {
                    mouse_button_cb((uint8_t)e.button.button, 0);
                }
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                if (menu) {
                    break;
                }
                extra_set_active_device(EXTRA_DEVICE_KBM);
                if (mouse_wheel_cb) {
                    mouse_wheel_cb((int)e.wheel.y);
                }
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
                if (!g_sdl_gamepad) {
                    g_sdl_gamepad = SDL_OpenGamepad(e.gdevice.which);
                    if (g_sdl_gamepad) {
                        extra_set_active_device(EXTRA_DEVICE_GAMEPAD);
                        g_sdl_prompt_generation++;
                        const char *layout = extra_get_gamepad_prompt_mode() == EXTRA_PROMPT_MODE_SWITCH ? "switch" : "xbox";
                        printf("[input] SDL3 gamepad connected: %s (layout=%s)\n", SDL_GetGamepadName(g_sdl_gamepad), layout);
                    }
                }
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                if (g_sdl_gamepad && e.gdevice.which == SDL_GetGamepadID(g_sdl_gamepad)) {
                    printf("[input] SDL3 gamepad disconnected\n");
                    SDL_CloseGamepad(g_sdl_gamepad);
                    g_sdl_gamepad = NULL;
                    extra_set_active_device(EXTRA_DEVICE_KBM);
                }
                break;
            default:
                break;
        }
    }
    if (extra_check_gamepad_overlay_hold()) {
        ac_overlay_toggle();
        if (ac_overlay_visible() || ac_reinfo_visible()) {
            extra_mouse_mode_suspend(g_sdl_window);
        } else if (g_sdl_input_focus) {
            extra_mouse_mode_resume(g_sdl_window);
        }
    }
    return !g_sdl_exit_requested;
}

static inline void extra_app_set_fullscreen(int *app_fs, int on) {
    if (app_fs) {
        *app_fs = on;
    }
    extra_window_set_fullscreen(on);
}

static inline void extra_app_pump_inline(void) {
    extra_window_poll_events(NULL, NULL, NULL, NULL);
}

static inline void *extra_app_open_window(HANDLE app_msg_ev, unsigned *app_foc, unsigned *msg_cnt, void (*enqueue_fn)(unsigned)) {
    (void)app_msg_ev;
    g_app_enqueue_fn = enqueue_fn;
    void *win = extra_window_ptr();
    if (win) {
        if (app_foc) {
            *app_foc = 1;
        }
        if (msg_cnt && enqueue_fn && !(*msg_cnt)) {
            enqueue_fn(EXTRA_APP_MSG_FOCUS);
        }
    }
    return win;
}
