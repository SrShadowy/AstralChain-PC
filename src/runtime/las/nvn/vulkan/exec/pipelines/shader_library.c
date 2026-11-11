// [extra]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <SDL3/SDL_timer.h>
#include "rt.h"
#include "../../shaders.h"
#include "../../exec_internal.h"

_Atomic unsigned long long gpl_stage_built, gpl_stage_hits, gpl_stage_ns;

#define STAGELIB_TAB 16384
static struct { uint64_t key; StageLib *lib; } stagelib_tab[STAGELIB_TAB];
static int stagelib_n;
static pthread_mutex_t stagelib_lock = PTHREAD_MUTEX_INITIALIZER;

// Depth clamp (nvnCommandBufferSetDepthClamp): Astral Chain turns it on in every command buffer and never off (counted
// through the opening cutscenes and play), so its pipelines clamp depth instead of clipping at the near and far planes,
// as on the Switch. Without it, an effect card the game places at the near plane in the opening helicopter cutscene (a
// black card that dims the view) was clipped in some frames and drawn in others as the camera moved: the whole picture
// flickered between two brightnesses. LARECOMP_VK_DEPTH_CLAMP=0: off.
VkBool32 game_depth_clamp(void) {
    static int on = -1;
    if (on < 0) { const char *e = getenv("LARECOMP_VK_DEPTH_CLAMP"); on = g_vk.depth_clamp && !(e && !atoi(e)); }
    return on ? VK_TRUE : VK_FALSE;
}

static VkPipeline gpl_build_stage(int stage, const uint32_t *spv, size_t n) {
    VkBindingInfo binds[MAX_BIND];
    int nb = vk_reflect(spv, n, binds, MAX_BIND);
    VkDescriptorSetLayout sl[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    sl[stage] = vk_set_layout(binds, nb, (uint32_t)stage, stage ? VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT);
    VkPipelineLayoutCreateInfo lci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                      .flags = VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT, .setLayoutCount = 2, .pSetLayouts = sl};
    VkPipelineLayout layout;
    if (vkCreatePipelineLayout(g_vk.dev, &lci, NULL, &layout) != VK_SUCCESS) return VK_NULL_HANDLE;
    VkShaderModule m = vk_module(spv, n);
    if (!m) return VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo ss = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                          .stage = stage ? VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT, .module = m,
                                          .pName = "main"};
    VkGraphicsPipelineLibraryCreateInfoEXT li = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
                                                 .flags = stage ? VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT
                                                                : VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT};
    VkPipelineRenderingCreateInfo ri = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .pNext = &li};
    VkPipelineViewportStateCreateInfo vs = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
    VkPipelineRasterizationStateCreateInfo rs = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                 .depthClampEnable = game_depth_clamp(), .polygonMode = VK_POLYGON_MODE_FILL, .lineWidth = 1};
    VkPipelineDepthStencilStateCreateInfo ds = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    VkPipelineDynamicStateCreateInfo dy = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                           .dynamicStateCount = NDYN, .pDynamicStates = DYN};
    VkGraphicsPipelineCreateInfo pi = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .pNext = &ri,
                                       .flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR, .stageCount = 1, .pStages = &ss,
                                       .pDynamicState = &dy, .layout = layout};
    if (stage) pi.pDepthStencilState = &ds;
    else { pi.pViewportState = &vs; pi.pRasterizationState = &rs; }
    VkPipeline lib = VK_NULL_HANDLE;
    uint64_t t0 = SDL_GetTicksNS();
    VkResult r = vkCreateGraphicsPipelines(g_vk.dev, g_vk.pcache, 1, &pi, NULL, &lib);
    atomic_fetch_add(&gpl_stage_ns, SDL_GetTicksNS() - t0);
    atomic_fetch_add(&gpl_stage_built, 1);
    atomic_fetch_add(&vk_pipelines_built, 1);  // (pipeline cache save)
    vkDestroyShaderModule(g_vk.dev, m, NULL);
    if (r != VK_SUCCESS) {
        static int nlog;
        if (nlog++ < 10) LOG("[vk] shader library failed: %d (stage %d)\n", r, stage);
        return VK_NULL_HANDLE;
    }
    return lib;
}

// The library of this shader (stage 0 vertex, 1 fragment), built here if no thread has yet; NULL if it cannot be.
StageLib *gpl_stage(int stage, const uint32_t *spv, size_t n) {
    if (!g_vk.gpl || !spv) return NULL;
    uint64_t key = hash_words(0x9E3779B97F4A7C15ull + (uint64_t)stage, spv, n * 4);
    if (!key) key = 1;
    pthread_mutex_lock(&stagelib_lock);
    uint32_t slot = (uint32_t)(key ^ (key >> 29)) & (STAGELIB_TAB - 1);
    while (stagelib_tab[slot].key && stagelib_tab[slot].key != key) slot = (slot + 1) & (STAGELIB_TAB - 1);
    StageLib *sl = stagelib_tab[slot].lib;
    bool build = false;
    if (!sl && stagelib_n < STAGELIB_TAB * 3 / 4) {
        sl = calloc(1, sizeof *sl);
        sl->key = key;
        stagelib_tab[slot].key = key;
        stagelib_tab[slot].lib = sl;
        stagelib_n++;
        build = true;
    }
    pthread_mutex_unlock(&stagelib_lock);
    if (!sl) return NULL;
    if (build) {
        sl->lib = gpl_build_stage(stage, spv, n);
        atomic_store(&sl->state, sl->lib ? 1 : 2);
    } else {
        atomic_fetch_add(&gpl_stage_hits, 1);
        for (uint64_t t0 = SDL_GetTicksNS(); atomic_load(&sl->state) == 0 && SDL_GetTicksNS() - t0 < 3000000000ull;) SDL_Delay(1);
    }
    return atomic_load(&sl->state) == 1 ? sl : NULL;
}

// (nvnProgramSetShaders) the game loaded a shader: its library, built on an idle worker with the keys nearly all
// draws use (shader_precompile.c: float attributes, plus unsigned attribute 2 for skinned meshes; every varying)
static uint64_t gpl_warm_seen[8192];
static int gpl_warm_nseen;
void gpl_warm_job(void *arg) {
    GplWarm *w = arg;
    for (int variant = 0; variant < 2; variant++) {
        ShaderKey k;
        memset(&k, 0, sizeof k);
        k.glsl = 1;
        if (w->stage) k.vs_out_mask = 0xFFFFFFFFu;
        if (variant) k.attr_kind[2] = 2;
        pthread_mutex_lock(&translate_lock);
        maxwell_gmem_base = HEAP_BASE;
        maxwell_gmem_size = HEAP_MAX;
        ShaderInfo *si = maxwell_translate((const uint8_t *)(uintptr_t)w->code, w->cmax, w->stage ? STAGE_FRAGMENT : STAGE_VERTEX, &k);
        pthread_mutex_unlock(&translate_lock);
        bool skinned = si && si->msl && !w->stage && (si->attr_mask & 4);
        if (si && si->msl) {
            size_t n = 0;
            uint32_t *spv = vk_glsl_to_spirv(si->msl, w->stage ? STAGE_FRAGMENT : STAGE_VERTEX, w->stage ? "fs" : "vs", &n);
            gpl_stage(w->stage, spv, n);
            free(spv);
        }
        if (si) maxwell_free(si);
        if (!skinned) break;
    }
    free(w);
}
void gpl_warm_stage(const NvnProgram *np, int s, uint64_t hash) {
    if (!g_vk.gpl || !np->stages[s].code_addr || !np->stages[s].const_offset) return;
    static int off = -1;
    if (off < 0) off = getenv("LARECOMP_NO_GPL_WARM") != NULL || sync_shaders();
    if (off) return;
    for (int i = 0; i < gpl_warm_nseen; i++) if (gpl_warm_seen[i] == hash) return;
    if (gpl_warm_nseen == 8192) return;
    gpl_warm_seen[gpl_warm_nseen++] = hash;
    GplWarm *w = malloc(sizeof *w);
    w->code = np->stages[s].code_addr;
    w->cmax = np->stages[s].const_offset;
    w->stage = s;
    run_job_low(gpl_warm_job, w);
}
// [extra]
