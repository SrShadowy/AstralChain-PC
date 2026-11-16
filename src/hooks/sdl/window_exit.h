// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] Asynchronous exit watchdog and termination trigger for SDL3 window.
#pragma once
#include <SDL3/SDL.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#define EXTRA_EXIT_WATCHDOG_US 400000
#define EXTRA_APP_MSG_EXIT 4

extern SDL_Window *g_sdl_window;
extern int g_sdl_exit_requested;

__attribute__((weak)) void (*g_app_enqueue_fn)(unsigned) = NULL;

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
