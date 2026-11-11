#pragma once
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdlib.h>
#include "additions/overlay/ac_overlay.h"

#define EXTRA_GAMEPAD_DEADZONE 8000
#define EXTRA_OVERLAY_HOLD_MS 2000
#define EXTRA_DEVICE_GAMEPAD 0
#define EXTRA_DEVICE_KBM 1

enum {
    EXTRA_PROMPT_MODE_SWITCH = 0,
    EXTRA_PROMPT_MODE_KBM = 1,
    EXTRA_PROMPT_MODE_XBOX = 2,
    EXTRA_PROMPT_MODE_AUTO = 3
};

enum {
    EXTRA_HID_BIT_A = 0,
    EXTRA_HID_BIT_B = 1,
    EXTRA_HID_BIT_X = 2,
    EXTRA_HID_BIT_Y = 3,
    EXTRA_HID_BIT_LSTICK = 4,
    EXTRA_HID_BIT_RSTICK = 5,
    EXTRA_HID_BIT_L = 6,
    EXTRA_HID_BIT_R = 7,
    EXTRA_HID_BIT_ZL = 8,
    EXTRA_HID_BIT_ZR = 9,
    EXTRA_HID_BIT_PLUS = 10,
    EXTRA_HID_BIT_MINUS = 11,
    EXTRA_HID_BIT_DLEFT = 12,
    EXTRA_HID_BIT_DUP = 13,
    EXTRA_HID_BIT_DRIGHT = 14,
    EXTRA_HID_BIT_DDOWN = 15
};

__attribute__((weak)) SDL_Gamepad *g_sdl_gamepad;
__attribute__((weak)) int g_sdl_active_device = EXTRA_DEVICE_KBM;
__attribute__((weak)) uint32_t g_sdl_prompt_generation = 1;
__attribute__((weak)) int g_extra_prompts_override = -1;
__attribute__((weak)) int g_extra_gamepad_confirm_a = -1;
__attribute__((weak)) uint64_t g_sdl_l3r3_start_ms = 0;
__attribute__((weak)) int g_sdl_l3r3_latched = 0;

static inline int extra_check_gamepad_overlay_hold(void) {
    if (!g_sdl_gamepad) {
        g_sdl_l3r3_start_ms = 0;
        g_sdl_l3r3_latched = 0;
        return 0;
    }
    bool l3 = SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_LEFT_STICK);
    bool r3 = SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    if (l3 && r3) {
        uint64_t now = SDL_GetTicks();
        if (g_sdl_l3r3_start_ms == 0) {
            g_sdl_l3r3_start_ms = now;
        } else if (!g_sdl_l3r3_latched && (now - g_sdl_l3r3_start_ms) >= EXTRA_OVERLAY_HOLD_MS) {
            g_sdl_l3r3_latched = 1;
            return 1;
        }
    } else {
        g_sdl_l3r3_start_ms = 0;
        g_sdl_l3r3_latched = 0;
    }
    return 0;
}

static inline int extra_get_active_device(void) {
    return g_sdl_active_device;
}

static inline uint32_t extra_get_prompt_generation(void) {
    return g_sdl_prompt_generation;
}

static inline int extra_gamepad_confirm_a_enabled(void) {
    if (g_extra_gamepad_confirm_a >= 0) {
        return g_extra_gamepad_confirm_a;
    }
    const char *e = getenv("AC_XBOX_CONFIRM_A");
    if (!e || !*e) {
        g_extra_gamepad_confirm_a = 1;
        return g_extra_gamepad_confirm_a;
    }
    g_extra_gamepad_confirm_a = (*e != '0') ? 1 : 0;
    return g_extra_gamepad_confirm_a;
}

static inline void extra_set_gamepad_confirm_a(int enabled) {
    int v = enabled ? 1 : 0;
    if (g_extra_gamepad_confirm_a != v) {
        g_extra_gamepad_confirm_a = v;
        g_sdl_prompt_generation++;
    }
}

static inline void extra_set_prompts_override(int mode) {
    if (g_extra_prompts_override != mode) {
        g_extra_prompts_override = mode;
        g_sdl_prompt_generation++;
    }
}

static inline void extra_set_active_device(int dev) {
    if (g_sdl_active_device != dev) {
        g_sdl_active_device = dev;
        g_sdl_prompt_generation++;
    }
}

static inline int extra_is_gamepad_connected(void) {
    return g_sdl_gamepad != NULL;
}

static inline int extra_get_gamepad_prompt_mode(void) {
    if (!g_sdl_gamepad) {
        return EXTRA_PROMPT_MODE_XBOX;
    }
    SDL_GamepadType t = SDL_GetGamepadType(g_sdl_gamepad);
    if (t == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO ||
        t == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT ||
        t == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT ||
        t == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR) {
        return EXTRA_PROMPT_MODE_SWITCH;
    }
    return EXTRA_PROMPT_MODE_XBOX;
}

static inline void extra_open_initial_gamepad(void) {
    if (g_sdl_gamepad) {
        return;
    }
    int count = 0;
    SDL_JoystickID *pads = SDL_GetGamepads(&count);
    if (pads && count > 0) {
        g_sdl_gamepad = SDL_OpenGamepad(pads[0]);
        if (g_sdl_gamepad) {
            extra_set_active_device(EXTRA_DEVICE_GAMEPAD);
            g_sdl_prompt_generation++;
            const char *layout = extra_get_gamepad_prompt_mode() == EXTRA_PROMPT_MODE_SWITCH ? "switch" : "xbox";
            printf("[input] SDL3 gamepad connected: %s (layout=%s)\n", SDL_GetGamepadName(g_sdl_gamepad), layout);
        }
    }
    if (pads) {
        SDL_free(pads);
    }
}

static inline void extra_poll_gamepad(uint64_t *buttons, int32_t *lx, int32_t *ly, int32_t *rx, int32_t *ry) {
    if (!g_sdl_gamepad || ac_overlay_visible()) {
        return;
    }
    uint64_t b = 0;
    int confirm_a = extra_gamepad_confirm_a_enabled();
    int south_bit = confirm_a ? EXTRA_HID_BIT_A : EXTRA_HID_BIT_B;
    int east_bit = confirm_a ? EXTRA_HID_BIT_B : EXTRA_HID_BIT_A;
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_SOUTH)) {
        b |= (1ull << south_bit);
    }
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_EAST)) {
        b |= (1ull << east_bit);
    }
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_WEST)) {
        b |= (1ull << EXTRA_HID_BIT_Y);
    }
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_NORTH)) {
        b |= (1ull << EXTRA_HID_BIT_X);
    }
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) {
        b |= (1ull << EXTRA_HID_BIT_L);
    }
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) {
        b |= (1ull << EXTRA_HID_BIT_R);
    }
    if (!g_sdl_l3r3_latched && SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_LEFT_STICK)) {
        b |= (1ull << EXTRA_HID_BIT_LSTICK);
    }
    if (!g_sdl_l3r3_latched && SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_RIGHT_STICK)) {
        b |= (1ull << EXTRA_HID_BIT_RSTICK);
    }
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_START)) {
        b |= (1ull << EXTRA_HID_BIT_PLUS);
    }
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_BACK)) {
        b |= (1ull << EXTRA_HID_BIT_MINUS);
    }
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) {
        b |= (1ull << EXTRA_HID_BIT_DLEFT);
    }
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP)) {
        b |= (1ull << EXTRA_HID_BIT_DUP);
    }
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
        b |= (1ull << EXTRA_HID_BIT_DRIGHT);
    }
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
        b |= (1ull << EXTRA_HID_BIT_DDOWN);
    }
    if (SDL_GetGamepadAxis(g_sdl_gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > EXTRA_GAMEPAD_DEADZONE) {
        b |= (1ull << EXTRA_HID_BIT_ZL);
    }
    if (SDL_GetGamepadAxis(g_sdl_gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > EXTRA_GAMEPAD_DEADZONE) {
        b |= (1ull << EXTRA_HID_BIT_ZR);
    }
    int16_t glx = SDL_GetGamepadAxis(g_sdl_gamepad, SDL_GAMEPAD_AXIS_LEFTX);
    int16_t gly = SDL_GetGamepadAxis(g_sdl_gamepad, SDL_GAMEPAD_AXIS_LEFTY);
    int16_t grx = SDL_GetGamepadAxis(g_sdl_gamepad, SDL_GAMEPAD_AXIS_RIGHTX);
    int16_t gry = SDL_GetGamepadAxis(g_sdl_gamepad, SDL_GAMEPAD_AXIS_RIGHTY);
    if (abs(glx) > EXTRA_GAMEPAD_DEADZONE || abs(gly) > EXTRA_GAMEPAD_DEADZONE) {
        *lx = glx;
        *ly = -gly;
    }
    if (abs(grx) > EXTRA_GAMEPAD_DEADZONE || abs(gry) > EXTRA_GAMEPAD_DEADZONE) {
        *rx = grx;
        *ry = -gry;
    }
    if (b || abs(glx) > EXTRA_GAMEPAD_DEADZONE || abs(gly) > EXTRA_GAMEPAD_DEADZONE ||
        abs(grx) > EXTRA_GAMEPAD_DEADZONE || abs(gry) > EXTRA_GAMEPAD_DEADZONE) {
        extra_set_active_device(EXTRA_DEVICE_GAMEPAD);
    }
    *buttons |= b;
}
