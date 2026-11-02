#pragma once
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <stdio.h>

static inline int extra_vk_create_surface(VkInstance inst, void *window_ptr, VkSurfaceKHR *surface) {
    SDL_Window *win = (SDL_Window *)window_ptr;
    if (!win) {
        fprintf(stderr, "[vk hook] invalid window pointer for Vulkan surface\n");
        return 0;
    }
    if (!SDL_Vulkan_CreateSurface(win, inst, NULL, surface)) {
        fprintf(stderr, "[vk hook] SDL_Vulkan_CreateSurface failed: %s\n", SDL_GetError());
        return 0;
    }
    return 1;
}

static inline const char *const *extra_vk_get_instance_extensions(uint32_t *count) {
    return SDL_Vulkan_GetInstanceExtensions(count);
}

static inline uint32_t extra_vk_collect_instance_extensions(const char **ext, uint32_t max_ext) {
    uint32_t n_sdl = 0;
    const char *const *sdl_ext = SDL_Vulkan_GetInstanceExtensions(&n_sdl);
    uint32_t n = 0;
    if (sdl_ext) {
        for (uint32_t i = 0; i < n_sdl && n < max_ext; i++) ext[n++] = sdl_ext[i];
    } else {
        if (n < max_ext) ext[n++] = VK_KHR_SURFACE_EXTENSION_NAME;
    }
    return n;
}

static inline void *extra_vk_init_warm(VkInstance inst, VkSurfaceKHR *surface, void (*create_inst_fn)(void)) {
    void *win = SDL_CreateWindow("AstralChainGpuWarm", 16, 16, SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);
    if (!win) {
        fprintf(stderr, "no window for the GPU warm-up: %s\n", SDL_GetError());
        exit(1);
    }
    if (create_inst_fn) create_inst_fn();
    if (!extra_vk_create_surface(inst, win, surface)) {
        fprintf(stderr, "surface creation for warm-up failed\n");
        exit(1);
    }
    return win;
}

