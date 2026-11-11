// [extra] Dear ImGui's Vulkan backend on our device, drawn inside the window
// image's dynamic rendering (adapted from LASRecomp runtime/nvn/vulkan/imgui_glue.cpp).
#include "imgui.h"
#include "imgui_impl_vulkan.h"

extern "C" {
#include "nvn/vulkan/vk.h"
}

constexpr uint32_t kImGuiDescriptorPoolSize = 64;
constexpr uint32_t kImGuiMinImageCount = 2;
constexpr uint32_t kImGuiMaxImageCount = 8;

extern "C" bool ac_vk_imgui_init(void) {
    if (ImGui::GetIO().BackendRendererUserData) {
        return true;
    }
    ImGui_ImplVulkan_InitInfo ii = {};
    ii.ApiVersion = VK_API_VERSION_1_3;
    ii.Instance = g_vk.inst;
    ii.PhysicalDevice = g_vk.phys;
    ii.Device = g_vk.dev;
    ii.QueueFamily = g_vk.qfam;
    ii.Queue = g_vk.queue;
    ii.DescriptorPoolSize = kImGuiDescriptorPoolSize;
    ii.MinImageCount = kImGuiMinImageCount;
    ii.ImageCount = kImGuiMaxImageCount;
    ii.PipelineCache = g_vk.pcache;
    ii.UseDynamicRendering = true;
    ii.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ii.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    ii.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    ii.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &g_vk.sc_format;
    ii.CheckVkResultFn = +[](VkResult r) {
        if (r != VK_SUCCESS) {
            fprintf(stderr, "[overlay] imgui vulkan error: %d\n", r);
        }
    };
    bool ok = ImGui_ImplVulkan_Init(&ii);
    if (!ok) {
        fprintf(stderr, "[overlay] ImGui Vulkan backend init failed\n");
    }
    return ok;
}

extern "C" const char *ac_vk_gpu_name(void) {
    return g_vk.props.deviceName;
}

extern "C" void ac_vk_extent(int *w, int *h) {
    if (w) *w = (int)g_vk.sc_extent.width;
    if (h) *h = (int)g_vk.sc_extent.height;
}

extern "C" void ac_vk_imgui_new_frame(void) {
    ImGui_ImplVulkan_NewFrame();
}

extern "C" void ac_vk_imgui_render(void *cmd) {
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), (VkCommandBuffer)cmd);
}

extern "C" void ac_vk_recreate_swapchain(void) {
    extern void dev_recreate_swapchain(void);
    dev_recreate_swapchain();
}
