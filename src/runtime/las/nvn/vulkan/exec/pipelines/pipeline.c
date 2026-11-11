// [extra]
#include <SDL3/SDL_timer.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../exec_internal.h"

// Build statistics
_Atomic unsigned long long build_ns_pipe;
_Atomic unsigned long long build_pipe_hits, build_pipe_misses, build_ns_pipe_miss;
unsigned long long pwarm_queued, pwarm_ready, pwarm_late, pwarm_rejected;
_Atomic unsigned long long gpl_linked, gpl_link_ns, gpl_opt_built;

#define PIPE_TAB 8192
static struct { uint64_t key; Pipe *pipe; } pipe_tab[PIPE_TAB];
static int pipe_tab_used;

static VkPrimitiveTopology topo_class_base(uint8_t c) {
    return c == 0 ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST : c == 1 ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

VkResult create_pipeline(const PipeKey *k, VkPipeline *out) {
    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = k->prog->vs, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = k->prog->fs, .pName = "main"}};
    VkVertexInputAttributeDescription va[32];
    VkVertexInputBindingDescription vb[17];
    VkVertexInputBindingDivisorDescriptionEXT vd[17];
    uint32_t na = 0, nb = 0, nd = 0;
    for (int i = 0; i < 32; i++)
        if (k->attr_mask & (1u << i)) va[na++] = (VkVertexInputAttributeDescription){(uint32_t)i, k->attr_bind[i], k->attr_fmt[i], k->attr_off[i]};
    for (uint32_t s = 0; s < 17; s++) {
        if (!(k->used_streams & (1u << s))) continue;
        bool inst = k->divisor[s] != 0;
        vb[nb++] = (VkVertexInputBindingDescription){s, k->stride[s], inst ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX};
        if (inst && k->divisor[s] > 1 && g_vk.divisor) vd[nd++] = (VkVertexInputBindingDivisorDescriptionEXT){s, k->divisor[s]};
    }
    VkPipelineVertexInputDivisorStateCreateInfoEXT dvi = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT,
                                                          .vertexBindingDivisorCount = nd, .pVertexBindingDivisors = vd};
    VkPipelineVertexInputStateCreateInfo vi = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, .pNext = nd ? &dvi : NULL,
                                               .vertexBindingDescriptionCount = nb, .pVertexBindingDescriptions = vb,
                                               .vertexAttributeDescriptionCount = na, .pVertexAttributeDescriptions = va};
    VkPipelineInputAssemblyStateCreateInfo ia = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                 .topology = topo_class_base(k->topo_class)};
    VkPipelineViewportStateCreateInfo vs = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
    VkPipelineRasterizationStateCreateInfo rs = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                 .depthClampEnable = game_depth_clamp(), .polygonMode = k->poly_fill ? VK_POLYGON_MODE_FILL : VK_POLYGON_MODE_LINE, .lineWidth = 1};
    VkPipelineMultisampleStateCreateInfo ms = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                               .rasterizationSamples = (VkSampleCountFlagBits)k->samples, .alphaToCoverageEnable = k->a2c};
    VkPipelineDepthStencilStateCreateInfo ds = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    VkPipelineColorBlendAttachmentState cba[8] = {0};
    for (uint32_t i = 0; i < k->ncolor; i++) {
        cba[i].colorWriteMask = k->write_mask[i];
        if ((k->blend_on >> i) & 1) {
            const uint8_t *b = k->blend[i];
            cba[i].blendEnable = VK_TRUE;
            cba[i].srcColorBlendFactor = blend_factor(b[1]);
            cba[i].dstColorBlendFactor = blend_factor(b[2]);
            cba[i].srcAlphaBlendFactor = blend_factor(b[3]);
            cba[i].dstAlphaBlendFactor = blend_factor(b[4]);
            cba[i].colorBlendOp = blend_op(b[5]);
            cba[i].alphaBlendOp = blend_op(b[6]);
        }
    }
    VkPipelineColorBlendStateCreateInfo cb = {.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                              .attachmentCount = k->ncolor, .pAttachments = cba};
    VkPipelineDynamicStateCreateInfo dy = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                           .dynamicStateCount = NDYN, .pDynamicStates = DYN};
    VkPipelineRenderingCreateInfo ri = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .colorAttachmentCount = k->ncolor,
                                        .pColorAttachmentFormats = k->color, .depthAttachmentFormat = k->depth,
                                        .stencilAttachmentFormat = k->stencil};
    VkGraphicsPipelineCreateInfo pi = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .pNext = &ri, .stageCount = 2,
                                       .pStages = stages, .pVertexInputState = &vi, .pInputAssemblyState = &ia, .pViewportState = &vs,
                                       .pRasterizationState = &rs, .pMultisampleState = &ms, .pDepthStencilState = &ds,
                                       .pColorBlendState = &cb, .pDynamicState = &dy, .layout = k->prog->layout};
    VkPipelineCreationFeedback fb_pipe = {0}, fb_stages[2] = {0};
    VkPipelineCreationFeedbackCreateInfo fb = {.sType = VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO,
                                               .pPipelineCreationFeedback = &fb_pipe, .pipelineStageCreationFeedbackCount = 2,
                                               .pPipelineStageCreationFeedbacks = fb_stages};
    ri.pNext = &fb;
    uint64_t bt0 = SDL_GetTicksNS();
    VkResult r = vkCreateGraphicsPipelines(g_vk.dev, g_vk.pcache, 1, &pi, NULL, out);
    uint64_t bdt = SDL_GetTicksNS() - bt0;
    atomic_fetch_add(&build_ns_pipe, bdt);
    if (fb_pipe.flags & VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT) {
        if (fb_pipe.flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT) atomic_fetch_add(&build_pipe_hits, 1);
        else { atomic_fetch_add(&build_pipe_misses, 1); atomic_fetch_add(&build_ns_pipe_miss, bdt); }
    }
    if (r != VK_SUCCESS) {
        static int n;
        if (n++ < 10) LOG("[vk] pipeline creation failed: %d\n", r);
    }
    atomic_fetch_add(&stat_compiles, 1); atomic_fetch_add(&vk_pipelines_built, 1);
    return r;
}

void build_pipeline(void *arg) {
    Pipe *pp = arg;
    VkResult r = create_pipeline(&pp->k, &pp->pipe);
    atomic_store(&pp->state, r == VK_SUCCESS ? 1 : 2);
}

static void build_pipeline_opt(void *arg) {
    Pipe *pp = arg;
    VkPipeline p = VK_NULL_HANDLE;
    if (create_pipeline(&pp->k, &p) == VK_SUCCESS) { atomic_store(&pp->opt, (uint64_t)(uintptr_t)p); atomic_fetch_add(&gpl_opt_built, 1); }
}

typedef struct { uint64_t key; VkPipeline lib; } StateLib;
#define STATELIB_TAB 4096
static StateLib vi_libs[STATELIB_TAB], fo_libs[STATELIB_TAB];

static VkPipeline state_lib(StateLib *tab, uint64_t key, const VkGraphicsPipelineCreateInfo *ci) {
    if (!key) key = 1;
    uint32_t slot = (uint32_t)(key ^ (key >> 29)) & (STATELIB_TAB - 1);
    for (int probe = 0; probe < 64; probe++, slot = (slot + 1) & (STATELIB_TAB - 1)) {
        if (tab[slot].key == key) return tab[slot].lib;
        if (!tab[slot].key) break;
    }
    VkPipeline lib = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(g_vk.dev, g_vk.pcache, 1, ci, NULL, &lib) != VK_SUCCESS) return VK_NULL_HANDLE;
    if (!tab[slot].key) { tab[slot].key = key; tab[slot].lib = lib; }
    return lib;
}

bool gpl_link(Pipe *pp) {
    const PipeKey *k = &pp->k;
    Prog *p = k->prog;
    if (!g_vk.gpl || !p->vs_lib || !p->fs_lib || !k->poly_fill) return false;
    uint64_t t0 = SDL_GetTicksNS();
    VkPipelineDynamicStateCreateInfo dy = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                           .dynamicStateCount = NDYN, .pDynamicStates = DYN};
    VkVertexInputAttributeDescription va[32];
    VkVertexInputBindingDescription vb[17];
    VkVertexInputBindingDivisorDescriptionEXT vd[17];
    uint32_t na = 0, nb = 0, nd = 0;
    for (int i = 0; i < 32; i++)
        if (k->attr_mask & (1u << i)) va[na++] = (VkVertexInputAttributeDescription){(uint32_t)i, k->attr_bind[i], k->attr_fmt[i], k->attr_off[i]};
    for (uint32_t s = 0; s < 17; s++) {
        if (!(k->used_streams & (1u << s))) continue;
        bool inst = k->divisor[s] != 0;
        vb[nb++] = (VkVertexInputBindingDescription){s, k->stride[s], inst ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX};
        if (inst && k->divisor[s] > 1 && g_vk.divisor) vd[nd++] = (VkVertexInputBindingDivisorDescriptionEXT){s, k->divisor[s]};
    }
    VkPipelineVertexInputDivisorStateCreateInfoEXT dvi = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT,
                                                          .vertexBindingDivisorCount = nd, .pVertexBindingDivisors = vd};
    VkPipelineVertexInputStateCreateInfo vi = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, .pNext = nd ? &dvi : NULL,
                                               .vertexBindingDescriptionCount = nb, .pVertexBindingDescriptions = vb,
                                               .vertexAttributeDescriptionCount = na, .pVertexAttributeDescriptions = va};
    VkPipelineInputAssemblyStateCreateInfo ia = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                 .topology = topo_class_base(k->topo_class)};
    VkGraphicsPipelineLibraryCreateInfoEXT vli = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
                                                  .flags = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT};
    VkGraphicsPipelineCreateInfo vci = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .pNext = &vli,
                                        .flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR, .pVertexInputState = &vi,
                                        .pInputAssemblyState = &ia, .pDynamicState = &dy};
    uint64_t vkey = hash_words(0x7631ull, k->attr_fmt, sizeof k->attr_fmt);
    vkey = hash_words(vkey, k->attr_off, sizeof k->attr_off);
    vkey = hash_words(vkey, k->attr_bind, sizeof k->attr_bind);
    uint32_t vmisc[3] = {k->attr_mask, k->used_streams, k->topo_class};
    vkey = hash_words(vkey, vmisc, sizeof vmisc);
    vkey = hash_words(vkey, k->stride, sizeof k->stride);
    vkey = hash_words(vkey, k->divisor, sizeof k->divisor);

    VkPipelineColorBlendAttachmentState cba[8] = {0};
    for (uint32_t i = 0; i < k->ncolor; i++) {
        cba[i].colorWriteMask = k->write_mask[i];
        if ((k->blend_on >> i) & 1) {
            const uint8_t *b = k->blend[i];
            cba[i].blendEnable = VK_TRUE;
            cba[i].srcColorBlendFactor = blend_factor(b[1]);
            cba[i].dstColorBlendFactor = blend_factor(b[2]);
            cba[i].srcAlphaBlendFactor = blend_factor(b[3]);
            cba[i].dstAlphaBlendFactor = blend_factor(b[4]);
            cba[i].colorBlendOp = blend_op(b[5]);
            cba[i].alphaBlendOp = blend_op(b[6]);
        }
    }
    VkPipelineColorBlendStateCreateInfo cb = {.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                              .attachmentCount = k->ncolor, .pAttachments = cba};
    VkPipelineMultisampleStateCreateInfo ms = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                               .rasterizationSamples = (VkSampleCountFlagBits)k->samples, .alphaToCoverageEnable = k->a2c};
    VkGraphicsPipelineLibraryCreateInfoEXT fli = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
                                                  .flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT};
    VkPipelineRenderingCreateInfo ri = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .pNext = &fli,
                                        .colorAttachmentCount = k->ncolor, .pColorAttachmentFormats = k->color,
                                        .depthAttachmentFormat = k->depth, .stencilAttachmentFormat = k->stencil};
    VkGraphicsPipelineCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .pNext = &ri,
                                        .flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR, .pColorBlendState = &cb,
                                        .pMultisampleState = &ms, .pDynamicState = &dy};
    uint64_t fkey = hash_words(0x666full, k->color, sizeof k->color);
    uint32_t fmisc[6] = {k->depth, k->stencil, k->ncolor, k->samples, k->a2c, k->blend_on};
    fkey = hash_words(fkey, fmisc, sizeof fmisc);
    fkey = hash_words(fkey, k->blend, sizeof k->blend);
    fkey = hash_words(fkey, k->write_mask, sizeof k->write_mask);
    VkPipeline libs[4] = {state_lib(vi_libs, vkey, &vci), p->vs_lib, p->fs_lib, state_lib(fo_libs, fkey, &fci)};
    if (!libs[0] || !libs[3]) return false;
    VkPipelineLibraryCreateInfoKHR lci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR, .libraryCount = 4, .pLibraries = libs};
    VkGraphicsPipelineCreateInfo pi = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .pNext = &lci, .layout = p->layout};
    VkResult r = vkCreateGraphicsPipelines(g_vk.dev, VK_NULL_HANDLE, 1, &pi, NULL, &pp->pipe);
    atomic_fetch_add(&gpl_link_ns, SDL_GetTicksNS() - t0);
    if (r != VK_SUCCESS) {
        static int nlog;
        if (nlog++ < 10) LOG("[vk] pipeline link failed: %d\n", r);
        return false;
    }
    atomic_fetch_add(&gpl_linked, 1);
    atomic_store(&pp->state, 1);
    run_job_low(build_pipeline_opt, pp);
    return true;
}

#define PREC_MAX 65536
#define PREC_MAGIC (0x45504950u ^ (uint32_t)sizeof(PipeRec))
static PipeRec *prec;
static int32_t *prec_next;
static int32_t prec_head[4096];
static int prec_n = -1, prec_cap;
static FILE *prec_out;
static pthread_mutex_t pwarm_lock = PTHREAD_MUTEX_INITIALIZER;
static Prog **pwarm_pending;
static int pwarm_n, pwarm_cap;

static bool pipe_warm_off(void) {
    static int off = -1;
    if (off < 0) off = getenv("LARECOMP_NO_PIPE_WARM") != NULL || sync_shaders();
    return off;
}

static int prec_find(uint64_t ident, const PipeKey *k) {
    for (int32_t i = prec_head[ident & 4095]; i; i = prec_next[i - 1])
        if (prec[i - 1].ident == ident && !memcmp(&prec[i - 1].k, k, sizeof *k)) return i - 1;
    return -1;
}

static bool prec_add(const PipeRec *r) {
    if (prec_n >= PREC_MAX) return false;
    if (prec_n == prec_cap) {
        int cap = prec_cap ? prec_cap * 2 : 1024;
        PipeRec *np = realloc(prec, (size_t)cap * sizeof *np);
        if (!np) return false;
        prec = np;
        int32_t *nn = realloc(prec_next, (size_t)cap * sizeof *nn);
        if (!nn) return false;
        prec_next = nn;
        prec_cap = cap;
    }
    prec[prec_n] = *r;
    prec_next[prec_n] = prec_head[r->ident & 4095];
    prec_head[r->ident & 4095] = ++prec_n;
    return true;
}

void prec_load(void) {
    prec_n = 0;
    FILE *f = fopen("cache/pipelines.bin", "rb");
    if (!f) return;
    uint32_t magic = 0;
    PipeRec r;
    if (fread(&magic, 4, 1, f) == 1 && magic == PREC_MAGIC)
        while (fread(&r, sizeof r, 1, f) == 1)
            if (prec_find(r.ident, &r.k) < 0 && !prec_add(&r)) break;
    fclose(f);
    if (magic != PREC_MAGIC) remove("cache/pipelines.bin");
    LOG("[vk] pipeline warm-up: %d recorded pipelines\n", prec_n);
}

int prec_count(void) {
    return prec_n;
}

const PipeRec *prec_entry(int i) {
    if (i >= 0 && i < prec_n) return &prec[i];
    return NULL;
}

int prec_old_pipelines(long bytes) {
    FILE *f = fopen("cache/pipelines.bin", "rb");
    if (!f) return 0;
    int n = 0;
    uint32_t magic = 0;
    PipeRec r;
    if (fread(&magic, 4, 1, f) == 1 && magic == PREC_MAGIC)
        while (ftell(f) + (long)sizeof r <= bytes && fread(&r, sizeof r, 1, f) == 1) {
            int i = prec_find(r.ident, &r.k);
            if (i >= n) n = i + 1;
        }
    fclose(f);
    return n;
}

bool prec_ident_needed_from(uint64_t ident, int old_pipes) {
    for (int32_t r = prec_head[ident & 4095]; r; r = prec_next[r - 1])
        if (r - 1 >= old_pipes && prec[r - 1].ident == ident) return true;
    return false;
}

void pipe_record(const Prog *p, const PipeKey *k) {
    if (pipe_warm_off()) return;
    if (prec_n < 0) prec_load();
    PipeRec r;
    memset(&r, 0, sizeof r);
    r.ident = p->ident;
    r.k = *k;
    r.k.prog = NULL;
    if (prec_find(r.ident, &r.k) >= 0 || !prec_add(&r)) return;
    if (!prec_out) {
        mkdir("cache", 0755);
        prec_out = fopen("cache/pipelines.bin", "ab");
        if (prec_out && fseek(prec_out, 0, SEEK_END) == 0 && ftell(prec_out) == 0) { uint32_t m = PREC_MAGIC; fwrite(&m, 4, 1, prec_out); }
    }
    if (prec_out) { fwrite(&r, sizeof r, 1, prec_out); fflush(prec_out); }
}

void pipe_warm_add(Prog *p) {
    if (pipe_warm_off()) return;
    pthread_mutex_lock(&pwarm_lock);
    if (pwarm_n == pwarm_cap) {
        int cap = pwarm_cap ? pwarm_cap * 2 : 256;
        Prog **np = realloc(pwarm_pending, (size_t)cap * sizeof *np);
        if (np) { pwarm_pending = np; pwarm_cap = cap; }
    }
    if (pwarm_n < pwarm_cap) pwarm_pending[pwarm_n++] = p;
    pthread_mutex_unlock(&pwarm_lock);
}

static bool format_has(VkFormat f, VkFormatFeatureFlags want) {
    static struct { VkFormat f; VkFormatFeatureFlags have; } known[64];
    static int nknown;
    for (int i = 0; i < nknown; i++) if (known[i].f == f) return (known[i].have & want) == want;
    VkFormatProperties p;
    vkGetPhysicalDeviceFormatProperties(g_vk.phys, f, &p);
    if (nknown < 64) { known[nknown].f = f; known[nknown++].have = p.optimalTilingFeatures; }
    return (p.optimalTilingFeatures & want) == want;
}

bool pipe_key_for_device(PipeKey *k) {
    extern VkFormat vk_depth24_format(void);
    if (k->ncolor > 8 || !k->samples || (k->samples & (k->samples - 1)) || k->samples > 64) return false;
    for (uint32_t i = 0; i < k->ncolor; i++) {
        if (k->color[i] == VK_FORMAT_UNDEFINED) continue;
        VkFormatFeatureFlags want = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
        if ((k->blend_on >> i) & 1) want |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
        if (!format_has(k->color[i], want)) return false;
    }
    if (k->depth == VK_FORMAT_D24_UNORM_S8_UINT) k->depth = vk_depth24_format();
    if (k->stencil == VK_FORMAT_D24_UNORM_S8_UINT) k->stencil = vk_depth24_format();
    if (k->depth && !format_has(k->depth, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) return false;
    if (k->stencil && !format_has(k->stencil, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) return false;
    for (int i = 0; i < 32; i++) {
        if (!(k->attr_mask & (1u << i))) continue;
        if (k->attr_bind[i] > ZERO_BINDING) return false;
        if (k->attr_bind[i] != ZERO_BINDING) k->attr_fmt[i] = vertex_format_supported(k->attr_fmt[i]);
    }
    for (int s = 0; s < 17; s++) if (k->divisor[s] > 1 && !g_vk.divisor) k->divisor[s] = 1;
    return k->topo_class <= 2;
}

static void build_pipeline_warm(void *arg) {
    Pipe *pp = arg;
    if (atomic_exchange(&pp->claimed, 1)) return;
    build_pipeline(pp);
}

void vk_pipeline_warmup(void) {
    if (pipe_warm_off()) return;
    if (prec_n < 0) prec_load();
    Prog *ready[256];
    int nready = 0;
    pthread_mutex_lock(&pwarm_lock);
    int keep = 0;
    for (int i = 0; i < pwarm_n; i++) {
        Prog *p = pwarm_pending[i];
        int s = atomic_load(&p->state);
        if (s == 0 || (s == 1 && nready == 256)) pwarm_pending[keep++] = p;
        else if (s == 1) ready[nready++] = p;
    }
    pwarm_n = keep;
    pthread_mutex_unlock(&pwarm_lock);
    for (int n = 0; n < nready; n++) {
        Prog *p = ready[n];
        for (int32_t i = prec_head[p->ident & 4095]; i; i = prec_next[i - 1]) {
            if (prec[i - 1].ident != p->ident) continue;
            if (pipe_tab_used >= PIPE_TAB * 3 / 4) return;
            PipeKey k = prec[i - 1].k;
            if (!pipe_key_for_device(&k)) { pwarm_rejected++; continue; }
            k.prog = p;
            uint64_t ph = hash_words(0xcbf29ce484222325ull, &k, sizeof k);
            if (!ph) ph = 1;
            uint32_t slot = (uint32_t)(ph ^ (ph >> 29)) & (PIPE_TAB - 1);
            while (pipe_tab[slot].key && pipe_tab[slot].key != ph) slot = (slot + 1) & (PIPE_TAB - 1);
            if (pipe_tab[slot].pipe) continue;
            Pipe *pp = calloc(1, sizeof *pp);
            pp->k = k;
            pp->warmed = true;
            pipe_tab[slot].key = ph;
            pipe_tab[slot].pipe = pp;
            pipe_tab_used++;
            pwarm_queued++;
            run_job_low(build_pipeline_warm, pp);
        }
    }
}

Pipe *pipe_lookup(const PipeKey *k, Prog *pr) {
    uint64_t ph = hash_words(0xcbf29ce484222325ull, k, sizeof *k);
    if (!ph) ph = 1;
    uint32_t pslot = (uint32_t)(ph ^ (ph >> 29)) & (PIPE_TAB - 1);
    while (pipe_tab[pslot].key && pipe_tab[pslot].key != ph) pslot = (pslot + 1) & (PIPE_TAB - 1);
    Pipe *pp = pipe_tab[pslot].pipe;
    if (!pp) {
        pp = calloc(1, sizeof *pp);
        pp->k = *k;
        pp->claimed = 1;
        pipe_tab[pslot].key = ph;
        pipe_tab[pslot].pipe = pp;
        pipe_tab_used++;
        pipe_record(pr, k);
        if (gpl_link(pp)) {}
        else if (skip_unbuilt()) run_job(build_pipeline, pp);
        else build_pipeline(pp);
    } else if (pp->warmed && !pp->drawn) {
        pp->drawn = true;
        if (atomic_load(&pp->state) == 0) pwarm_late++; else pwarm_ready++;
        if (atomic_load(&pp->state) == 0 && !skip_unbuilt() && !atomic_exchange(&pp->claimed, 1) && !gpl_link(pp)) build_pipeline(pp);
    }
    wait_built(&pp->state);
    return pp;
}

void pipe_reset(void) {
    memset(pipe_tab, 0, sizeof pipe_tab);
    pipe_tab_used = 0;
}
// [extra]
