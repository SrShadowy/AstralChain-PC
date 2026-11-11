/* [extra] In-game ImGui overlay (Insert): performance/graphics/debug menu drawn
 * into the window image between vk_present_draw and vk_frame_end. Pure C ABI
 * so the C trampoline (window_trampoline.h) and present path (postprocess.c)
 * can drive it without including ImGui or Vulkan headers. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void ac_overlay_set_window(void *sdl_window);
void ac_overlay_process_event(const void *sdl_event); /* SDL_Event* */
void ac_overlay_toggle(void);
int ac_overlay_visible(void);

/* Present-path hooks (device/postprocess.c): pre_frame runs before
 * vk_frame_begin (applies pending swapchain recreation); frame runs while the
 * window image's dynamic rendering is live, right before vk_frame_end. */
void ac_overlay_pre_frame(void);
void ac_overlay_frame(void);

#ifdef __cplusplus
}
#endif
