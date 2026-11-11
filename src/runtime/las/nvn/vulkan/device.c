// [extra]
// Vulkan device, window, swapchain and presentation orchestrator.
// Modularized into device/ domain components (instance, physical, logical, surface, swapchain, present, postprocess). // [extra]

#include "device_internal.h" // [extra]

VkG g_vk;
Settings g_settings; // [extra]
DeviceState g_dev_state; // [extra]

void backend_init(void) {
    dev_window_init(); // [extra]
    LOG("[vk] %s, swapchain %ux%u\n", g_vk.props.deviceName, g_vk.sc_extent.width, g_vk.sc_extent.height);
}

void backend_clear_window(void) {
    static const float black[4] = {0, 0, 0, 1};
    if (g_vk.dev && vk_frame_begin(black)) vk_frame_end(NULL);
}
// [extra]
