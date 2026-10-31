// [extra]
#include "../device_internal.h"

VkPresentModeKHR dev_present_mode(void) {
    // Presentation modes: // [extra]
    // - mailbox: Triple buffering / low latency without tearing (Recommended on modern Linux/Wayland)
    // - fifo: Strict VSync (traditional 60 Hz lock)
    // - fifo_relaxed: Adaptive VSync (syncs at refresh rate, tears when falling below to avoid stutter)
    // - immediate: No sync (lowest latency, tears) // [extra]
    const char *e = getenv("LARECOMP_VK_PRESENT");
    VkPresentModeKHR want = VK_PRESENT_MODE_MAILBOX_KHR; // [extra]
    if (e) {
        if (!strcmp(e, "immediate")) want = VK_PRESENT_MODE_IMMEDIATE_KHR;
        else if (!strcmp(e, "fifo")) want = VK_PRESENT_MODE_FIFO_KHR;
        else if (!strcmp(e, "fifo_relaxed") || !strcmp(e, "fifo-relaxed")) want = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        else if (!strcmp(e, "mailbox")) want = VK_PRESENT_MODE_MAILBOX_KHR;
    } // [extra]
    uint32_t n = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_vk.phys, g_vk.surface, &n, NULL);
    VkPresentModeKHR m[16];
    if (n > 16) n = 16;
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_vk.phys, g_vk.surface, &n, m);
    VkPresentModeKHR got = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0; i < n; i++) if (m[i] == want) got = want;
    static VkPresentModeKHR logged = (VkPresentModeKHR)-1;
    if (got != logged) {
        logged = got;
        const char *name = got == VK_PRESENT_MODE_MAILBOX_KHR ? "mailbox" // [extra]
                         : got == VK_PRESENT_MODE_IMMEDIATE_KHR ? "immediate"
                         : got == VK_PRESENT_MODE_FIFO_RELAXED_KHR ? "fifo_relaxed" : "fifo";
        LOG("[vk] present mode %s%s\n", name, got != want ? " (the one asked for is not supported)" : ""); // [extra]
    }
    return got;
}

void dev_create_swapchain(void) {
    VkSurfaceCapabilitiesKHR caps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_vk.phys, g_vk.surface, &caps));
    int w, h;
    dev_window_size(&w, &h);
    VkExtent2D ext = caps.currentExtent.width != UINT32_MAX ? caps.currentExtent : (VkExtent2D){(uint32_t)w, (uint32_t)h};
    if (!ext.width || !ext.height) return;  // minimised
    uint32_t nf = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_vk.phys, g_vk.surface, &nf, NULL);
    VkSurfaceFormatKHR fmts[64];
    if (nf > 64) nf = 64;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_vk.phys, g_vk.surface, &nf, fmts);
    // scanout shows the bytes the game wrote (see the raw views in the present pass): a UNORM swapchain like Metal's
    VkSurfaceFormatKHR sf = fmts[0];
    for (uint32_t i = 0; i < nf; i++)
        if (fmts[i].format == VK_FORMAT_B8G8R8A8_UNORM && fmts[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) { sf = fmts[i]; break; }
    uint32_t count = caps.minImageCount + 1;
    if (caps.maxImageCount && count > caps.maxImageCount) count = caps.maxImageCount;
    if (count > 8) count = 8;
    VkSwapchainKHR old = g_vk.swapchain;
    VkSwapchainCreateInfoKHR ci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, .surface = g_vk.surface, .minImageCount = count,
        .imageFormat = sf.format, .imageColorSpace = sf.colorSpace, .imageExtent = ext, .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, .presentMode = dev_present_mode(), .clipped = VK_TRUE,
        .oldSwapchain = old};
    VK_CHECK(vkCreateSwapchainKHR(g_vk.dev, &ci, NULL, &g_vk.swapchain));
    for (uint32_t i = 0; i < g_vk.sc_count; i++) vkDestroyImageView(g_vk.dev, g_vk.sc_views[i], NULL);
    if (old) vkDestroySwapchainKHR(g_vk.dev, old, NULL);
    g_vk.sc_format = sf.format;
    g_vk.sc_extent = ext;
    g_vk.sc_count = 8;
    vkGetSwapchainImagesKHR(g_vk.dev, g_vk.swapchain, &g_vk.sc_count, g_vk.sc_images);
    for (uint32_t i = 0; i < g_vk.sc_count; i++) {
        VkImageViewCreateInfo vi = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = g_vk.sc_images[i],
                                    .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = sf.format,
                                    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
        VK_CHECK(vkCreateImageView(g_vk.dev, &vi, NULL, &g_vk.sc_views[i]));
    }
}

void dev_recreate_swapchain(void) {
    vkDeviceWaitIdle(g_vk.dev);
    dev_create_swapchain();
}
// [extra]
