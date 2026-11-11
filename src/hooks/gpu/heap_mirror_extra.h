#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* [extra] Architectural Constants & Redundant Copy Elimination for Heap Mirror.
 *
 * In Astral Chain, shader global memory (maxwell_gmem_base = HEAP_BASE) is mirrored
 * in device-local GPU memory (VMA auto prefer device). Dirty pages written by the CPU
 * are uploaded to the mirror prior to submission (gmem_sync).
 *
 * During compute passes (e.g. tile light clustering, compute culling), shaders write
 * into storage buffers (SSBOs) located in the heap mirror. Under naive eager write-back,
 * every completed submission would stage and memcpy several large buffers (e.g. 9.6 MB
 * and 16.8 MB light lists) back into guest memory at ~2 GB/s, even though the CPU never
 * inspects this data (it is consumed strictly by subsequent GPU fragment passes).
 *
 * This module defines clean architectural constants and provides an active filter
 * to eliminate these redundant copies, drastically lowering PCIe traffic, memory
 * allocation churn (vmaCreateBuffer/vmaDestroyBuffer), and CPU utilization.
 */

#define EXTRA_PAGE_SIZE_4K             4096ULL
#define EXTRA_PAGE_OFFSET_MASK_4K      (EXTRA_PAGE_SIZE_4K - 1ULL)
#define EXTRA_PAGE_MASK_4K             (~EXTRA_PAGE_OFFSET_MASK_4K)
#define EXTRA_PAGE_SHIFT_4K            12
#define EXTRA_MB_SHIFT                 20
#define EXTRA_MAX_DIRTY_PAGES          65536
#define EXTRA_MAX_RUN_PAGES            16384       /* Max contiguous pages per copy run (64 MB) */
#define EXTRA_MAX_WB_PENDING           64          /* Max GPU write-back ranges recorded per submission */
#define EXTRA_MAX_WB_FLIGHT            256         /* Max in-flight GPU write-back staging buffers */
#define EXTRA_LAZY_TAB_MAX             512         /* Max entries in lazy write-back table */
#define EXTRA_LAZY_HOT_MAX             1024        /* Max entries in hot ranges table */
#define EXTRA_LAZY_UNEXPECTED_LOG_MAX  20
#define EXTRA_HEAP_ALIGN_64K           0xFFFFULL   /* 64 KB alignment for heap allocation requests */
#define EXTRA_MIRROR_COPY_ALIGN        16          /* Alignment for transient staging allocations */
#define EXTRA_ZERO_BUFFER_ALIGN        256         /* Alignment for fallback zero buffer */
#define EXTRA_ZERO_FALLBACK_WORDS      1024        /* 4 KB zero buffer fallback words */
#define EXTRA_DEFAULT_WB_THRESHOLD     (1ULL << 20)/* 1 MB threshold: skips GPU-internal compute buffers */

enum ExtraWbMode {
    EXTRA_WB_MODE_FILTER = 0,   /* Default: filter out large GPU compute buffers (light lists, culling) */
    EXTRA_WB_MODE_DISABLED = 1, /* Completely disable write-back (0 staging buffers, 0 memcpy) */
    EXTRA_WB_MODE_ALL = 2       /* Legacy: write back all storage buffers eagerly */
};

static inline int extra_wb_resolve_mode(void) {
    static int cached_mode = -1;
    if (cached_mode >= 0) return cached_mode;

    const char *e = getenv("LARECOMP_VK_WB");
    if (e) {
        if (!strcmp(e, "0") || !strcmp(e, "none") || !strcmp(e, "NONE") ||
            !strcmp(e, "off") || !strcmp(e, "OFF")) {
            cached_mode = EXTRA_WB_MODE_DISABLED;
            fprintf(stderr, "[vk] write-back mode: disabled (zero staging copies)\n");
        } else if (!strcmp(e, "all") || !strcmp(e, "1") || !strcmp(e, "legacy")) {
            cached_mode = EXTRA_WB_MODE_ALL;
            fprintf(stderr, "[vk] write-back mode: legacy all\n");
        } else {
            cached_mode = EXTRA_WB_MODE_FILTER;
        }
    } else {
        cached_mode = EXTRA_WB_MODE_FILTER;
    }
    return cached_mode;
}

static inline uint64_t extra_wb_resolve_threshold(void) {
    static uint64_t cached_threshold = 0;
    if (cached_threshold > 0) return cached_threshold;

    const char *t = getenv("LARECOMP_VK_WB_THRESHOLD");
    if (t) {
        uint64_t val = (uint64_t)strtoull(t, NULL, 0);
        if (val > 0) {
            cached_threshold = val;
            return cached_threshold;
        }
    }
    cached_threshold = EXTRA_DEFAULT_WB_THRESHOLD;
    return cached_threshold;
}

/* Returns true if the storage buffer should be skipped from write-back. */
static inline bool extra_wb_should_skip(uint64_t size) {
    int mode = extra_wb_resolve_mode();
    if (mode == EXTRA_WB_MODE_DISABLED) return true;
    if (mode == EXTRA_WB_MODE_FILTER) {
        uint64_t threshold = extra_wb_resolve_threshold();
        if (size >= threshold) return true;
    }
    return false;
}

/* Fast-path check: returns true if there are in-flight writebacks or staging buffers to reap. */
static inline bool extra_wb_has_work(int nflight, int nfree) {
    return (nflight > 0 || nfree > 0);
}

#ifdef VK_VERSION_1_0
#define EXTRA_WB_POOL_MAX 128

typedef struct {
    uint64_t cap;
    VkBuffer buf;
    VmaAllocation mem;
    void *ptr;
} ExtraWbPoolEntry;

static ExtraWbPoolEntry g_extra_wb_pool[EXTRA_WB_POOL_MAX];
static int g_extra_wb_npool;
static pthread_mutex_t g_extra_wb_pool_lock = PTHREAD_MUTEX_INITIALIZER;

/* Allocates or recycles a 4KB-aligned host-visible write-back staging buffer. */
static inline void extra_wb_pool_alloc(uint64_t size, VkBuffer *out_buf, VmaAllocation *out_mem, void **out_ptr) {
    uint64_t cap = (size + EXTRA_PAGE_SIZE_4K - 1ULL) & EXTRA_PAGE_MASK_4K;
    if (cap == 0) cap = EXTRA_PAGE_SIZE_4K;
    pthread_mutex_lock(&g_extra_wb_pool_lock);
    for (int i = g_extra_wb_npool - 1; i >= 0; i--) {
        if (g_extra_wb_pool[i].cap == cap) {
            ExtraWbPoolEntry e = g_extra_wb_pool[i];
            g_extra_wb_pool[i] = g_extra_wb_pool[--g_extra_wb_npool];
            pthread_mutex_unlock(&g_extra_wb_pool_lock);
            *out_buf = e.buf;
            *out_mem = e.mem;
            *out_ptr = e.ptr;
            return;
        }
    }
    pthread_mutex_unlock(&g_extra_wb_pool_lock);

    VkBufferCreateInfo bi = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                             .size = cap,
                             .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    VmaAllocationCreateInfo ai = {.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                                  .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT};
    VmaAllocationInfo info;
    VK_CHECK(vmaCreateBuffer(g_vk.vma, &bi, &ai, out_buf, out_mem, &info));
    *out_ptr = info.pMappedData;
}

/* Returns a completed write-back staging buffer to the pool, or evicts the oldest entry if full. */
static inline void extra_wb_pool_free(uint64_t size, VkBuffer buf, VmaAllocation mem, void *ptr) {
    if (!buf || !mem) return;
    uint64_t cap = (size + EXTRA_PAGE_SIZE_4K - 1ULL) & EXTRA_PAGE_MASK_4K;
    if (cap == 0) cap = EXTRA_PAGE_SIZE_4K;
    VkBuffer evict_buf = VK_NULL_HANDLE;
    VmaAllocation evict_mem = NULL;
    pthread_mutex_lock(&g_extra_wb_pool_lock);
    if (g_extra_wb_npool < EXTRA_WB_POOL_MAX) {
        g_extra_wb_pool[g_extra_wb_npool++] = (ExtraWbPoolEntry){cap, buf, mem, ptr};
    } else {
        evict_buf = g_extra_wb_pool[0].buf;
        evict_mem = g_extra_wb_pool[0].mem;
        g_extra_wb_pool[0] = (ExtraWbPoolEntry){cap, buf, mem, ptr};
    }
    pthread_mutex_unlock(&g_extra_wb_pool_lock);
    if (evict_buf) vmaDestroyBuffer(g_vk.vma, evict_buf, evict_mem);
}

static inline void extra_wb_pool_reset(void) {
    pthread_mutex_lock(&g_extra_wb_pool_lock);
    for (int i = 0; i < g_extra_wb_npool; i++) {
        vmaDestroyBuffer(g_vk.vma, g_extra_wb_pool[i].buf, g_extra_wb_pool[i].mem);
    }
    g_extra_wb_npool = 0;
    pthread_mutex_unlock(&g_extra_wb_pool_lock);
}
#endif

