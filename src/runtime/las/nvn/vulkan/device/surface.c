// [extra]
#include "../device_internal.h"

void dev_window_size(int *w, int *h) {
#if defined(_WIN32)
    RECT r;
    if (!g_dev_state.window || !GetClientRect(g_dev_state.window, &r) || IsIconic(g_dev_state.window)) { *w = *h = 0; return; }
    *w = r.right - r.left;
    *h = r.bottom - r.top;
#else
    extra_window_get_size_hwnd(g_dev_state.window, w, h);
#endif
}

void dev_window_init(void) {
    if (g_dev_state.window) return;
    settings_load();
    g_dev_state.window = (HWND)nvn_host_window();
    if (!g_dev_state.window) FATAL("no host window for Vulkan presentation\n");
    atexit(dev_exit_handler);
    dev_create_instance();
#if defined(_WIN32)
    VkWin32SurfaceCreateInfoKHR si = {.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
                                      .hinstance = GetModuleHandleW(NULL), .hwnd = g_dev_state.window};
    VK_CHECK(vkCreateWin32SurfaceKHR(g_vk.inst, &si, NULL, &g_vk.surface));
#else
    if (!extra_vk_create_surface(g_vk.inst, g_dev_state.window, &g_vk.surface)) FATAL("extra_vk_create_surface failed\n");
#endif
    dev_pick_device();
    dev_create_device();
    dev_create_swapchain();
    vk_exec_init();
    vk_shaders_init();
}

// Every file below folder, recursively (exec.c vk_gpu_warm: the game's shader files).
void vk_walk_files(const char *folder, void (*fn)(const char *path, void *ctx), void *ctx) {
#if defined(_WIN32)
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof pattern, "%s/*", folder);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.cFileName[0] == '.') continue;
        char path[MAX_PATH];
        snprintf(path, sizeof path, "%s/%s", folder, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) vk_walk_files(path, fn, ctx);
        else fn(path, ctx);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    extra_storage_walk_files(folder, fn, ctx);
#endif
}

// AC_GPU_WARM (exec.c vk_gpu_warm): Vulkan on the GPU the game uses, without the game's window or a swapchain (a
// hidden window gives the surface the device choice checks presentation against).
void vk_init_for_warm(void) {
    if (g_dev_state.window) return;
#if defined(_WIN32)
    WNDCLASSW wc = {.lpfnWndProc = DefWindowProcW, .hInstance = GetModuleHandleW(NULL), .lpszClassName = L"AstralChainGpuWarm"};
    RegisterClassW(&wc);
    g_dev_state.window = CreateWindowExW(0, wc.lpszClassName, L"", WS_POPUP, 0, 0, 16, 16, NULL, NULL, wc.hInstance, NULL);
    if (!g_dev_state.window) FATAL("no window for the GPU warm-up\n");
    dev_create_instance();
    VkWin32SurfaceCreateInfoKHR si = {.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
                                      .hinstance = GetModuleHandleW(NULL), .hwnd = g_dev_state.window};
    VK_CHECK(vkCreateWin32SurfaceKHR(g_vk.inst, &si, NULL, &g_vk.surface));
#else
    g_dev_state.window = extra_vk_init_warm(g_vk.inst, &g_vk.surface, dev_create_instance);
#endif
    dev_pick_device();
    dev_create_device();
    vk_exec_init();
    vk_shaders_init();
}
// [extra]
