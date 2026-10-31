// [extra]
#include "../device_internal.h"

VkSemaphore dev_binary_semaphore(void) {
    VkSemaphoreCreateInfo si = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore s;
    VK_CHECK(vkCreateSemaphore(g_vk.dev, &si, NULL, &s));
    return s;
}

void dev_image_barrier(VkCommandBuffer cb, VkImage img, VkImageLayout from, VkImageLayout to) {
    VkImageMemoryBarrier2 b = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                               .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                               .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                               .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                               .oldLayout = from, .newLayout = to, .image = img,
                               .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                               .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED};
    VkDependencyInfo di = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &b};
    vkCmdPipelineBarrier2(cb, &di);
}

// Acquire the next window image and begin rendering into it (cleared to `clear`). Returns false when there is
// nothing to draw into (minimised window).
bool vk_frame_begin(const float clear[4]) {
    g_dev_state.cur_frame.ok = false;
    int w, h;
    dev_window_size(&w, &h);
    if (!w || !h) return false;  // minimised
    if (!g_vk.swapchain || (uint32_t)w != g_vk.sc_extent.width || (uint32_t)h != g_vk.sc_extent.height) dev_recreate_swapchain();
    if (!g_vk.swapchain) return false;
    static int trace = -1;
    if (trace < 0) trace = getenv("LARECOMP_VK_TRACE") != NULL;
    if (trace) LOG("[vk] frame begin %dx%d (swapchain %ux%u)\n", w, h, g_vk.sc_extent.width, g_vk.sc_extent.height);
    g_dev_state.slot = (g_dev_state.slot + 1) % FRAMES;
    if (!g_dev_state.acquire_sem[g_dev_state.slot]) g_dev_state.acquire_sem[g_dev_state.slot] = dev_binary_semaphore();
    vk_wait_seq(g_dev_state.slot_seq[g_dev_state.slot]);
    VkResult r = vkAcquireNextImageKHR(g_vk.dev, g_vk.swapchain, UINT64_MAX, g_dev_state.acquire_sem[g_dev_state.slot], VK_NULL_HANDLE, &g_dev_state.cur_frame.image);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
        dev_recreate_swapchain();
        r = vkAcquireNextImageKHR(g_vk.dev, g_vk.swapchain, UINT64_MAX, g_dev_state.acquire_sem[g_dev_state.slot], VK_NULL_HANDLE, &g_dev_state.cur_frame.image);
    }
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) { LOG("[vk] acquire failed: %d\n", r); return false; }
    g_dev_state.cur_frame.acquired = g_dev_state.acquire_sem[g_dev_state.slot];
    g_dev_state.slot_seq[g_dev_state.slot] = vk_pending_seq();
    VkCommandBuffer cb = vk_cmd();
    dev_image_barrier(cb, g_vk.sc_images[g_dev_state.cur_frame.image], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo ca = {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = g_vk.sc_views[g_dev_state.cur_frame.image],
                                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                    .clearValue.color.float32 = {clear[0], clear[1], clear[2], clear[3]}};
    VkRenderingInfo ri = {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO, .renderArea = {{0, 0}, g_vk.sc_extent}, .layerCount = 1,
                          .colorAttachmentCount = 1, .pColorAttachments = &ca};
    vkCmdBeginRendering(cb, &ri);
    VkViewport vp = {0, 0, (float)g_vk.sc_extent.width, (float)g_vk.sc_extent.height, 0, 1};
    VkRect2D sc = {{0, 0}, g_vk.sc_extent};
    vkCmdSetViewport(cb, 0, 1, &vp);
    vkCmdSetScissor(cb, 0, 1, &sc);
    g_dev_state.cur_frame.ok = true;
    return true;
}

// Passes of their own between vk_frame_begin and vk_frame_end (exec.c post passes): end the window image's
// rendering, and begin it again keeping what was drawn.
void vk_frame_suspend(void) {
    if (g_dev_state.cur_frame.ok) vkCmdEndRendering(vk_cmd());
}
void vk_frame_resume(void) {
    if (!g_dev_state.cur_frame.ok) return;
    VkCommandBuffer cb = vk_cmd();
    VkRenderingAttachmentInfo ca = {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = g_vk.sc_views[g_dev_state.cur_frame.image],
                                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD, .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
    VkRenderingInfo ri = {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO, .renderArea = {{0, 0}, g_vk.sc_extent}, .layerCount = 1,
                          .colorAttachmentCount = 1, .pColorAttachments = &ca};
    vkCmdBeginRendering(cb, &ri);
    VkViewport vp = {0, 0, (float)g_vk.sc_extent.width, (float)g_vk.sc_extent.height, 0, 1};
    VkRect2D sc = {{0, 0}, g_vk.sc_extent};
    vkCmdSetViewport(cb, 0, 1, &vp);
    vkCmdSetScissor(cb, 0, 1, &sc);
}

VkExtent2D vk_frame_extent(void) { return g_vk.sc_extent; }
void *vk_frame_cmd(void) { return (void *)vk_cmd(); }

static void save_window_image(VkCommandBuffer cb, VkSpan *out) {
    uint32_t w = g_vk.sc_extent.width, h = g_vk.sc_extent.height;
    *out = vk_readback((uint64_t)w * h * 4);
    dev_image_barrier(cb, g_vk.sc_images[g_dev_state.cur_frame.image], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkBufferImageCopy rg = {.bufferOffset = out->off, .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                            .imageExtent = {w, h, 1}};
    vkCmdCopyImageToBuffer(cb, g_vk.sc_images[g_dev_state.cur_frame.image], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, out->buf, 1, &rg);
    dev_image_barrier(cb, g_vk.sc_images[g_dev_state.cur_frame.image], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

// End rendering into the window image and present it. shot_path: also save the window image as PNG.
void vk_frame_end(const char *shot_path) {
    if (!g_dev_state.cur_frame.ok) return;
    VkCommandBuffer cb = vk_cmd();
    vkCmdEndRendering(cb);
    VkSpan shot = {0};
    if (shot_path) save_window_image(cb, &shot);
    dev_image_barrier(cb, g_vk.sc_images[g_dev_state.cur_frame.image], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    uint32_t img = g_dev_state.cur_frame.image;
    if (!g_dev_state.done_sem[img]) g_dev_state.done_sem[img] = dev_binary_semaphore();
    uint64_t seq = vk_pending_seq();
    vk_submit_extra(g_dev_state.cur_frame.acquired, g_dev_state.done_sem[img], VK_NULL_HANDLE);
    VkPresentInfoKHR pi = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, .waitSemaphoreCount = 1, .pWaitSemaphores = &g_dev_state.done_sem[img],
                           .swapchainCount = 1, .pSwapchains = &g_vk.swapchain, .pImageIndices = &img};
    VkResult r = vkQueuePresentKHR(g_vk.queue, &pi);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) dev_recreate_swapchain();
    g_dev_state.cur_frame.ok = false;
    if (shot_path) {
        vk_wait_seq(seq);
        uint32_t w = g_vk.sc_extent.width, h = g_vk.sc_extent.height;
        uint8_t *px = shot.ptr;
        bool bgr = g_vk.sc_format == VK_FORMAT_B8G8R8A8_UNORM || g_vk.sc_format == VK_FORMAT_B8G8R8A8_SRGB;
        for (uint64_t i = 0; bgr && i < (uint64_t)w * h; i++) { uint8_t t = px[i * 4]; px[i * 4] = px[i * 4 + 2]; px[i * 4 + 2] = t; }
        for (uint64_t i = 0; i < (uint64_t)w * h; i++) px[i * 4 + 3] = 255;
        vk_save_png(shot_path, px, w, h);
    }
}

// Astral Chain: uncompressed PNG (stored deflate blocks) instead of SDL_SavePNG.
static uint32_t png_crc(uint32_t c, const uint8_t *p, size_t n) {
    for (size_t i = 0; i < n; i++) { c ^= p[i]; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & -(c & 1)); }
    return c;
}
static void png_chunk(FILE *f, const char *type, const uint8_t *data, uint32_t n) {
    uint8_t len[4] = {n >> 24, n >> 16, n >> 8, n};
    fwrite(len, 1, 4, f);
    fwrite(type, 1, 4, f);
    if (n) fwrite(data, 1, n, f);
    uint32_t c = ~png_crc(png_crc(~0u, (const uint8_t *)type, 4), data, n);
    uint8_t crc[4] = {c >> 24, c >> 16, c >> 8, c};
    fwrite(crc, 1, 4, f);
}
void vk_save_png(const char *path, const uint8_t *rgba, uint32_t w, uint32_t h) {
    {
        static int dark_done;
        const char *e = getenv("LARECOMP_DEBUG_IF_DARK");
        float box[6] = {0, 0, 0, 0, 0, 0};
        extern uint64_t vk_frame_counter(void);
        if (e && !dark_done && sscanf(e, "%f,%f,%f,%f,%f,%f", &box[0], &box[1], &box[2], &box[3], &box[4], &box[5]) >= 5 &&
            vk_frame_counter() >= (uint64_t)box[5]) {
            uint32_t x0 = (uint32_t)(box[0] * w), y0 = (uint32_t)(box[1] * h), x1 = (uint32_t)(box[2] * w), y1 = (uint32_t)(box[3] * h);
            double sum = 0;
            uint64_t n = 0;
            for (uint32_t y = y0; y < y1 && y < h; y++)
                for (uint32_t x = x0; x < x1 && x < w; x++, n++) {
                    const uint8_t *p = rgba + ((uint64_t)y * w + x) * 4;
                    sum += 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
                }
            if (n && sum / n < box[4]) {
                g_debug_frame_request = 1;
                dark_done = 1;
                LOG("[debug] %s: box brightness %.1f below %.1f, dumping the next frame\n", path, sum / n, box[4]);
            }
        }
    }
    FILE *f = fopen(path, "wb");
    size_t row = (size_t)w * 4 + 1, raw = row * h, blocks = (raw + 65534) / 65535;
    uint8_t *z = f ? malloc(2 + raw + blocks * 5 + 4) : NULL;
    if (!z) { if (f) fclose(f); LOG("[vk] saving %s failed\n", path); return; }
    static const uint8_t sig[8] = {137, 'P', 'N', 'G', 13, 10, 26, 10};
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13] = {w >> 24, w >> 16, w >> 8, w, h >> 24, h >> 16, h >> 8, h, 8, 6, 0, 0, 0};
    png_chunk(f, "IHDR", ihdr, 13);
    size_t o = 0, done = 0;
    uint32_t a = 1, b = 0;
    z[o++] = 0x78; z[o++] = 0x01;
    while (done < raw) {
        size_t n = raw - done < 65535 ? raw - done : 65535;
        z[o++] = done + n == raw;
        z[o++] = n; z[o++] = n >> 8; z[o++] = ~n; z[o++] = ~n >> 8;
        for (size_t i = 0; i < n; i++, done++) {
            size_t y = done / row, x = done % row;
            uint8_t v = x ? rgba[y * w * 4 + x - 1] : 0;
            z[o++] = v;
            a = (a + v) % 65521;
            b = (b + a) % 65521;
        }
    }
    uint32_t ad = (b << 16) | a;
    z[o++] = ad >> 24; z[o++] = ad >> 16; z[o++] = ad >> 8; z[o++] = ad;
    png_chunk(f, "IDAT", z, (uint32_t)o);
    png_chunk(f, "IEND", NULL, 0);
    free(z);
    if (fclose(f)) LOG("[vk] saving %s failed\n", path);
    else LOG("[vk] saved %s (%ux%u)\n", path, w, h);
}
// [extra]
