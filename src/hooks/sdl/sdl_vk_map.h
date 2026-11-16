// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] Translation from SDL_Keycode to virtual key codes (VK_*).
#pragma once
#include <SDL3/SDL.h>
#include <stdint.h>
#include "hooks/input/vk_codes.h"

#define EXTRA_VK_MAX_ASCII_KEY 256

static inline uint8_t extra_sdl_to_vk(SDL_Keycode key) {
    if (key >= SDLK_A && key <= SDLK_Z) {
        return (uint8_t)('A' + (key - SDLK_A));
    }
    if (key >= SDLK_0 && key <= SDLK_9) {
        return (uint8_t)('0' + (key - SDLK_0));
    }
    if (key >= SDLK_F1 && key <= SDLK_F12) {
        return (uint8_t)(VK_F1 + (key - SDLK_F1));
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
        case SDLK_CAPSLOCK:
            return VK_CAPITAL;
        case SDLK_INSERT:
            return VK_INSERT;
        case SDLK_DELETE:
            return VK_DELETE;
        case SDLK_HOME:
            return VK_HOME;
        case SDLK_END:
            return VK_END;
        case SDLK_PAGEUP:
            return VK_PRIOR;
        case SDLK_PAGEDOWN:
            return VK_NEXT;
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
        default:
            return key < EXTRA_VK_MAX_ASCII_KEY ? (uint8_t)key : 0;
    }
}
