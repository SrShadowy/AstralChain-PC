// [extra]
#pragma once
#if defined(_WIN32)
#include <windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#else
#include "extra/hooks.h"
#endif
#include <math.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <SDL3/SDL_timer.h>

#include "../../settings.h"
#include "vk.h"
#include "../renderer.h"

#define FRAMES 3

typedef struct {
    uint32_t image;
    VkSemaphore acquired;
    bool ok;
} Frame;

typedef struct DeviceState {
    HWND window;
    VkDebugUtilsMessengerEXT messenger;
    bool fault_ext;
    _Atomic int pcache_saving;
    VkSemaphore acquire_sem[FRAMES];
    VkSemaphore done_sem[8];
    uint64_t slot_seq[FRAMES];
    int slot;
    Frame cur_frame;
    uint64_t frames;
    uint64_t total_frames;
    uint64_t frame_counter;
    float shot_crop[4];
    int present_crop[4];
    double last_fps_time;
    NvnTexture *last_presented;
} DeviceState;

extern DeviceState g_dev_state;

// Declarations between device submodules
void dev_create_instance(void);
bool dev_has_ext(VkExtensionProperties *e, uint32_t n, const char *name);
void dev_pick_device(void);
void dev_create_device(void);
void dev_pipeline_cache_save_async(void);
void dev_exit_handler(void);
void dev_window_size(int *w, int *h);
void dev_window_init(void);
VkPresentModeKHR dev_present_mode(void);
void dev_create_swapchain(void);
void dev_recreate_swapchain(void);
VkSemaphore dev_binary_semaphore(void);
void dev_image_barrier(VkCommandBuffer cb, VkImage img, VkImageLayout from, VkImageLayout to);
void dev_postprocess_present(TexBackend *b, NvnTexture *t, uint64_t t0);

// Frame lifecycle declarations
bool vk_frame_begin(const float clear[4]);
void vk_frame_suspend(void);
void vk_frame_resume(void);
void vk_frame_end(const char *shot_path);
VkExtent2D vk_frame_extent(void);
void *vk_frame_cmd(void);

// External engine declarations
void *nvn_host_window(void);
void vk_exec_init(void);
void vk_shaders_init(void);
void renderer_submit(void);
void renderer_set_scale(float sx, float sy);
void renderer_save_texture_tex(TexBackend *b, const char *path);
extern void vk_present_draw(VkCommandBuffer cb, TexBackend *b, float vx, float vy, float vw, float vh, const float pv[8]);
extern void vk_prof_frame(uint64_t frame, uint64_t present_ns);
extern void vk_pace_frame(void);
extern void vk_level_change_check(void);
extern _Atomic unsigned long long vk_pipelines_built;
extern void vk_texture_warmup(void);
extern void vk_pipeline_warmup(void);
extern bool g_debug_frame;
extern volatile int g_debug_frame_request;
extern bool g_gpuprof_frame;
extern void vk_gpuprof_report(void);
// [extra]
