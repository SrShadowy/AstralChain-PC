// [extra]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "kernel.h"
#include "rt.h"
#include "../../exec_internal.h"
#include "extra/core/heap_mirror_extra.h" // [extra]

// Global memory (shader LDG/STG, maxwell_gmem_base = HEAP_BASE): the guest heap imported as a GPU buffer
// (VK_EXT_external_memory_host), zero-copy like metal.m's g_guest_buffer, so compute results land in guest
// memory. The import pins its pages, so it covers only the heap the game set up (SetHeapSize), made on first use.
// LARECOMP_VK_NO_GMEM=1: no import, reads return zero.
VkBuffer gmem_buf;
static VkDeviceMemory gmem_mem;
uint64_t gmem_size;
uint64_t stat_mirror_bytes, stat_wb_bytes, stat_wb_ranges, stat_wb_faults;
int lazy_n;  // lazy write-back entries (gmem_writeback_apply)

static bool gmem_import(uint64_t size) {
    PFN_vkGetMemoryHostPointerPropertiesEXT props =
        (PFN_vkGetMemoryHostPointerPropertiesEXT)vkGetDeviceProcAddr(g_vk.dev, "vkGetMemoryHostPointerPropertiesEXT");
    VkMemoryHostPointerPropertiesEXT mp = {.sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT};
    VkResult pr = props ? props(g_vk.dev, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, (void *)(uintptr_t)HEAP_BASE, &mp) : VK_ERROR_EXTENSION_NOT_PRESENT;
    if (pr != VK_SUCCESS || !mp.memoryTypeBits) {
        LOG("[vk] heap import: host pointer properties %d types %#x base %#llx\n", pr, mp.memoryTypeBits, (unsigned long long)HEAP_BASE);
        return false;
    }
    VkImportMemoryHostPointerInfoEXT imp = {.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT,
                                            .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT,
                                            .pHostPointer = (void *)(uintptr_t)HEAP_BASE};
    VkMemoryAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = &imp, .allocationSize = size,
                               .memoryTypeIndex = (uint32_t)__builtin_ctz(mp.memoryTypeBits)};
    VkDeviceMemory mem;
    // Astral Chain: no-access heap pages (stack/transfer-memory sources) are made pinnable for the import.
    extern int nvn_heap_prepare_import(uint64_t size);
    extern void nvn_heap_finish_import(int imported);
    VkResult ar = nvn_heap_prepare_import(size) ? vkAllocateMemory(g_vk.dev, &ai, NULL, &mem) : VK_ERROR_MEMORY_MAP_FAILED;
    nvn_heap_finish_import(ar == VK_SUCCESS);
    if (ar != VK_SUCCESS) {
        LOG("[vk] heap import: allocate %llu bytes type %u -> %d\n", (unsigned long long)size, ai.memoryTypeIndex, ar);
        return false;
    }
    VkExternalMemoryBufferCreateInfo eb = {.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,
                                           .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT};
    VkBufferCreateInfo bi = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .pNext = &eb, .size = size,
                             .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT};
    VkBuffer buf;
    if (vkCreateBuffer(g_vk.dev, &bi, NULL, &buf) != VK_SUCCESS) { vkFreeMemory(g_vk.dev, mem, NULL); return false; }
    vkBindBufferMemory(g_vk.dev, buf, mem, 0);
    if (gmem_buf) {  // the heap grew: the old import may still be in use by submitted work
        vk_wait_seq(submit_seq);
        vkDestroyBuffer(g_vk.dev, gmem_buf, NULL);
        vkFreeMemory(g_vk.dev, gmem_mem, NULL);
    }
    gmem_buf = buf;
    gmem_mem = mem;
    gmem_size = size;
    LOG("[vk] guest heap imported for shader global memory: %llu MB\n", (unsigned long long)(size >> 20));
    return true;
}

static uint64_t gmem_tried;  // heap size the import was last attempted for (gmem_info imports / re-imports)

// ---- Astral Chain: device-local mirror of the guest heap (default; the host-pointer import is opt-in)
// The GPU reads its own copy of the heap. Before each submission the pages the CPU wrote since the last copy
// (runtime write tracking) are copied in through the upload buffer. Ranges that storing shaders may write are
// copied back after the submission and written into guest memory once it completes (gmem_writeback_apply),
// before a guest thread waiting on its fence resumes. No guest memory is pinned or handed to the driver.
extern int nvn_heap_mirror_dirty(uint64_t limit, uint64_t *pages, int max);
extern void nvn_heap_mirror_mark_all(uint64_t limit);
extern void nvn_heap_mirror_clean(uint64_t addr, uint64_t size);
extern void nvn_heap_mirror_interest(uint64_t addr, uint64_t size);
extern int nvn_guest_readable(uint64_t addr, uint64_t size);
static VmaAllocation gmem_alloc;  // non-null: gmem_buf is the mirror

static bool gmem_mirror(uint64_t size) {
    VkBufferCreateInfo bi = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size,
                             .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT};
    VmaAllocationCreateInfo ai = {.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
    VkBuffer buf;
    VmaAllocation mem;
    VkResult r = vmaCreateBuffer(g_vk.vma, &bi, &ai, &buf, &mem, NULL);
    if (r != VK_SUCCESS) { LOG("[vk] heap mirror: %llu MB buffer failed (%d)\n", (unsigned long long)(size >> 20), r); return false; }
    if (gmem_buf) {  // the heap grew: the old mirror may still be in use by submitted work
        vk_wait_seq(submit_seq);
        if (gmem_alloc) vmaDestroyBuffer(g_vk.vma, gmem_buf, gmem_alloc);
        else { vkDestroyBuffer(g_vk.dev, gmem_buf, NULL); vkFreeMemory(g_vk.dev, gmem_mem, NULL); }
    }
    gmem_buf = buf;
    gmem_alloc = mem;
    gmem_size = size;
    nvn_heap_mirror_mark_all(size);  // the whole committed heap is copied in by the next sync
    LOG("[vk] guest heap mirrored in device memory for shader global memory: %llu MB\n", (unsigned long long)(size >> 20));
    return true;
}

void gmem_sync(void) {
    if (!gmem_buf || !gmem_alloc) return;
    static uint64_t pages[65536];
    VkCommandBuffer cb = VK_NULL_HANDLE;
    for (;;) {
        int n = nvn_heap_mirror_dirty(gmem_size, pages, 65536);
        for (int i = 0; i < n;) {
            int j = i + 1;
            while (j < n && pages[j] == pages[j - 1] + 4096 && j - i < 16384) j++;  // runs of up to 64 MB
            uint64_t addr = pages[i], len = (uint64_t)(j - i) * 4096;
            if (nvn_guest_readable(addr, len)) {  // (no-access stack/transfer sources keep their old copy)
                if (!cb) cb = vk_upload_cmd();
                VkSpan sp = vk_transient(len, 16);
                memcpy(sp.ptr, (const void *)(uintptr_t)addr, len);
                VkBufferCopy cp = {sp.off, addr - HEAP_BASE, len};
                vkCmdCopyBuffer(cb, sp.buf, gmem_buf, 1, &cp);
                stat_mirror_bytes += len;
            }
            i = j;
        }
        if (n < 65536) break;
    }
    if (cb) vk_full_barrier(cb);  // mirror copies land before the vertex cache copies from the mirror
}

// Ranges storing shaders may have written in the pending submission; copied back at its end.
typedef struct { uint64_t seq, addr, size; VkBuffer buf; VmaAllocation mem; void *ptr; } GmemWb;
static GmemWb wb_pending[64], wb_flight[256];
static int wb_npend, wb_nflight;
void gmem_wb_note(uint64_t addr, uint64_t size) {
    if (!gmem_alloc || !size || addr < HEAP_BASE || addr - HEAP_BASE + size > gmem_size) return;
    if (extra_wb_should_skip(size)) return; // [extra]
    for (int i = 0; i < wb_npend; i++)
        if (wb_pending[i].addr == addr && wb_pending[i].size == size) return;
    if (wb_npend == 64) FATAL("heap mirror: more than 64 GPU-written ranges in one submission\n");
    wb_pending[wb_npend++] = (GmemWb){.addr = addr, .size = size};
}
void gmem_wb_record(VkCommandBuffer cb, uint64_t seq) {
    if (!wb_npend) return;
    vk_full_barrier(cb);
    for (int i = 0; i < wb_npend; i++) {
        GmemWb *w = &wb_pending[i];
        VkBufferCreateInfo bi = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = w->size, .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT};
        VmaAllocationCreateInfo ai = {.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                                      .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT};
        VmaAllocationInfo info;
        VK_CHECK(vmaCreateBuffer(g_vk.vma, &bi, &ai, &w->buf, &w->mem, &info));
        w->ptr = info.pMappedData;
        w->seq = seq;
        VkBufferCopy cp = {w->addr - HEAP_BASE, 0, w->size};
        vkCmdCopyBuffer(cb, gmem_buf, w->buf, 1, &cp);
        if (wb_nflight == 256) FATAL("heap mirror: too many GPU write-backs in flight\n");
        wb_flight[wb_nflight++] = *w;
    }
    wb_npend = 0;
    VkMemoryBarrier2 mb = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2, .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                           .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT, .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
                           .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT};
    VkDependencyInfo di = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &mb};
    vkCmdPipelineBarrier2(cb, &di);
}

// Lazy write-back (Astral Chain). Copying every storing shader's bound buffers back after each submission moved
// 0.25-1 GB/s through memcpy (run 414: ~10% of the guest thread), nearly all GPU-only data (light lists). A completed
// write-back instead stays in its host-visible staging buffer and its guest pages are made PAGE_NOACCESS. A CPU
// access faults and resolves that one page: every pending entry covering it copies its part in, oldest first (newer
// results win), and the page's protection is lifted.
// Protection changes split the heap's VAD and made GetWriteWatch (track_harvest) several times slower (run 416: 18%),
// so churn is avoided: a range the CPU has faulted on is "hot" and written back eagerly from then on (no protection);
// a newer write-back of exactly the same range takes over the older entry and re-protects only the pages that were
// resolved; pages of older entries whose bytes the new range covers completely are dropped without a copy. File I/O on
// guest buffers resolves its range first (nvn_lazy_flush_hook). LARECOMP_VK_LAZY_WB=0: copy at completion.
typedef struct { uint64_t addr, size, seq, lo, hi; VkBuffer buf; VmaAllocation mem; void *ptr; uint8_t *done; uint32_t left, faults; } Lazy;
enum { LAZY_MAX = 512, LAZY_HOT_MAX = 1024 };
static Lazy lazy_tab[LAZY_MAX], lazy_free[LAZY_MAX];
static int lazy_nfree;
static struct { uint64_t addr, size; } lazy_hot[LAZY_HOT_MAX];  // ranges the CPU reads: eager write-back
static int lazy_nhot;
static pthread_mutex_t lazy_lock = PTHREAD_MUTEX_INITIALIZER;
extern int nvn_page_protect(uint64_t addr, uint64_t size, int noaccess);
extern void (*nvn_lazy_flush_hook)(uint64_t addr, uint64_t size);
extern int (*nvn_lazy_fault_hook)(uint64_t addr);

static bool lazy_is_hot(uint64_t addr, uint64_t size) {
    for (int i = 0; i < lazy_nhot; i++) if (lazy_hot[i].addr == addr && lazy_hot[i].size == size) return true;
    return false;
}
// Entries with every page resolved move to lazy_free (staging destroyed by the GPU thread). Caller holds lazy_lock.
static void lazy_compact(void) {
    int k = 0;
    for (int i = 0; i < lazy_n; i++) {
        if (!lazy_tab[i].left) { free(lazy_tab[i].done); lazy_free[lazy_nfree++] = lazy_tab[i]; stat_wb_ranges++; continue; }
        lazy_tab[k++] = lazy_tab[i];
    }
    lazy_n = k;
}
static bool lazy_pending(uint64_t p) {
    for (int i = 0; i < lazy_n; i++)
        if (p >= lazy_tab[i].lo && p < lazy_tab[i].hi && !lazy_tab[i].done[(p - lazy_tab[i].lo) >> 12]) return true;
    return false;
}
// Copy page p's pending parts in, oldest entry first, and lift its protection. Caller holds lazy_lock and compacts.
static void lazy_resolve_page(uint64_t p, bool fault) {
    if (!nvn_page_protect(p, 4096, 0)) FATAL("lazy write-back: unprotect %llx failed\n", (unsigned long long)p);
    for (uint64_t last = 0;;) {
        int best = -1;
        for (int i = 0; i < lazy_n; i++) {
            Lazy *e = &lazy_tab[i];
            if (p < e->lo || p >= e->hi || e->done[(p - e->lo) >> 12] || e->seq < last) continue;
            if (best < 0 || e->seq < lazy_tab[best].seq) best = i;
        }
        if (best < 0) break;
        Lazy *e = &lazy_tab[best];
        uint64_t a = p > e->addr ? p : e->addr, b = p + 4096 < e->addr + e->size ? p + 4096 : e->addr + e->size;
        memcpy((void *)(uintptr_t)a, (const uint8_t *)e->ptr + (a - e->addr), b - a);
        nvn_heap_mirror_clean(a, b - a);  // (the mirror holds these bytes already)
        stat_wb_bytes += b - a;
        e->done[(p - e->lo) >> 12] = 1;
        e->left--;
        last = e->seq;
        // Hot: faults resolved at least half of the range's pages (it is read through; copying it at once is cheaper).
        if (fault && ++e->faults * 2 >= (uint32_t)((e->hi - e->lo) >> 12) && !lazy_is_hot(e->addr, e->size) && lazy_nhot < LAZY_HOT_MAX)
            lazy_hot[lazy_nhot++] = (typeof(lazy_hot[0])){e->addr, e->size};
    }
}
// Pending pages of [na, nb): a page whose pending bytes all lie inside [na, nb) is dropped (a newer write-back of
// [na, nb) replaces them; it stays protected if keep), any other is resolved. Caller holds lazy_lock.
static void lazy_supersede(uint64_t na, uint64_t nb, bool keep) {
    for (uint64_t p = na & ~4095ull; p < nb; p += 4096) {
        if (!lazy_pending(p)) continue;
        bool covered = true;
        for (int i = 0; i < lazy_n && covered; i++) {
            Lazy *e = &lazy_tab[i];
            if (p < e->lo || p >= e->hi || e->done[(p - e->lo) >> 12]) continue;
            uint64_t a = p > e->addr ? p : e->addr, b = p + 4096 < e->addr + e->size ? p + 4096 : e->addr + e->size;
            covered = a >= na && b <= nb;
        }
        if (!covered) { lazy_resolve_page(p, false); continue; }
        for (int i = 0; i < lazy_n; i++) {
            Lazy *e = &lazy_tab[i];
            if (p >= e->lo && p < e->hi && !e->done[(p - e->lo) >> 12]) { e->done[(p - e->lo) >> 12] = 1; e->left--; }
        }
        if (!keep && !nvn_page_protect(p, 4096, 0)) FATAL("lazy write-back: unprotect %llx failed\n", (unsigned long long)p);
    }
    lazy_compact();
}
static int lazy_fault(uint64_t addr) {
    extern int nvn_page_state(uint64_t addr);
    pthread_mutex_lock(&lazy_lock);
    uint64_t p = addr & ~4095ull;
    int hit = lazy_pending(p);
    if (hit) { stat_wb_faults++; lazy_resolve_page(p, true); lazy_compact(); }
    else if (p >= HEAP_BASE && p - HEAP_BASE < HEAP_MAX) {
        // Not pending. Guest threads run in parallel: another one may have resolved the page between this fault and
        // the lock (it is accessible now: retry the access). A no-access page that no entry holds is a bookkeeping
        // gap: lift the protection rather than stop the game (one run died here at the end of the highway chase).
        int state = nvn_page_state(p);
        if (state == 1) hit = 1;
        else if (state == 2 && nvn_page_protect(p, 4096, 0)) {
            static int nlog;
            if (nlog++ < 20) LOG("[vk] lazy write-back: unexpected protected page %llx lifted\n", (unsigned long long)p);
            hit = 1;
        }
    }
    pthread_mutex_unlock(&lazy_lock);
    return hit;
}
static void lazy_flush(uint64_t addr, uint64_t size) {
    pthread_mutex_lock(&lazy_lock);
    for (uint64_t p = addr & ~4095ull; p < addr + size; p += 4096)
        if (lazy_pending(p)) lazy_resolve_page(p, false);
    lazy_compact();
    pthread_mutex_unlock(&lazy_lock);
}

// Write the GPU's results of completed submissions into guest memory (before a fence waiter resumes).
void gmem_writeback_apply(uint64_t done) {
    if (!extra_wb_has_work(wb_nflight, lazy_nfree)) return; // [extra]
    static int lazy_on = -1;
    if (lazy_on < 0) {
        const char *e = getenv("LARECOMP_VK_LAZY_WB");
#if defined(_WIN32)
        lazy_on = !e || atoi(e) != 0;
#else
        lazy_on = e ? atoi(e) != 0 : 0;
#endif
        if (lazy_on) { nvn_lazy_fault_hook = lazy_fault; nvn_lazy_flush_hook = lazy_flush; }
    }
    pthread_mutex_lock(&lazy_lock);
    int k = 0;
    for (int i = 0; i < wb_nflight; i++) {
        GmemWb *w = &wb_flight[i];
        if (w->seq > done) { wb_flight[k++] = *w; continue; }
        if (!nvn_guest_readable(w->addr, w->size)) { vmaDestroyBuffer(g_vk.vma, w->buf, w->mem); continue; }
        nvn_heap_mirror_clean(w->addr, w->size);
        uint64_t lo = w->addr & ~4095ull, hi = (w->addr + w->size + 4095) & ~4095ull;
        uint32_t pages = (uint32_t)((hi - lo) >> 12);
        bool lazy = lazy_on && !lazy_is_hot(w->addr, w->size);
        int same = -1;
        bool overlap = false;
        for (int j = 0; j < lazy_n; j++)
            if (lazy_tab[j].lo < hi && lo < lazy_tab[j].hi) {
                if (lazy_tab[j].addr == w->addr && lazy_tab[j].size == w->size) same = j;
                else overlap = true;
            }
        if (lazy && same >= 0 && !overlap) {  // takes over the older entry: re-protect the pages it had resolved
            Lazy *o = &lazy_tab[same];
            for (uint32_t a = 0; a < pages;) {
                if (!o->done[a]) { a++; continue; }
                uint32_t b = a;
                while (b < pages && o->done[b]) b++;
                if (!nvn_page_protect(lo + ((uint64_t)a << 12), (uint64_t)(b - a) << 12, 1)) FATAL("lazy write-back: protect failed\n");
                a = b;
            }
            memset(o->done, 0, pages);
            lazy_free[lazy_nfree++] = *o;
            o->seq = w->seq; o->buf = w->buf; o->mem = w->mem; o->ptr = w->ptr; o->left = pages; o->faults = 0;
            continue;
        }
        if (same >= 0 || overlap) lazy_supersede(w->addr, w->addr + w->size, lazy);
        uint8_t *bits = lazy && lazy_n < LAZY_MAX ? calloc(pages, 1) : NULL;
        if (bits && nvn_page_protect(lo, hi - lo, 1)) {
            lazy_tab[lazy_n++] = (Lazy){w->addr, w->size, w->seq, lo, hi, w->buf, w->mem, w->ptr, bits, pages};
            continue;
        }
        free(bits);
        // (Table full or protection refused: pages superseded above stay protected without an entry; lift them. No
        // other entry has pending bytes inside this range any more.)
        if (lazy && (same >= 0 || overlap) && !nvn_page_protect(lo, hi - lo, 0)) FATAL("lazy write-back: unprotect failed\n");
        memcpy((void *)(uintptr_t)w->addr, w->ptr, w->size);  // eager: hot range, lazy off, or no room
        nvn_heap_mirror_clean(w->addr, w->size);  // (after the copy: the mirror holds these bytes already)
        stat_wb_bytes += w->size;
        stat_wb_ranges++;
        vmaDestroyBuffer(g_vk.vma, w->buf, w->mem);
    }
    wb_nflight = k;
    for (int i = 0; i < lazy_nfree; i++) vmaDestroyBuffer(g_vk.vma, lazy_free[i].buf, lazy_free[i].mem);
    lazy_nfree = 0;
    pthread_mutex_unlock(&lazy_lock);
}

VkDescriptorBufferInfo gmem_info(void) {
    static int off = -1;
    // Astral Chain: the device-local mirror by default. The host-pointer import (AC_VK_HEAP_IMPORT=1) made the
    // NVIDIA driver log bursts of GPU faults (nvlddmkm 153) and the machine bug-checked (0x119, 2026-10-19).
    static int import = -1;
    if (import < 0) import = getenv("AC_VK_HEAP_IMPORT") && g_vk.host_import;
    if (off < 0) off = getenv("LARECOMP_VK_NO_GMEM") != NULL;
    uint64_t want = (kernel_heap_size() + 0xFFFF) & ~0xFFFFull;
    if (want > HEAP_MAX) want = HEAP_MAX;
    if (!off && want > gmem_size && !(import ? gmem_import(want) : gmem_mirror(want))) {
        LOG("[vk] guest heap for shader global memory unavailable (%llu MB): reads return zero\n", (unsigned long long)(want >> 20));
        off = 1;
    }
    if (gmem_buf) return (VkDescriptorBufferInfo){gmem_buf, 0, gmem_size};
    static uint32_t zero[1024];
    VkSpan s = vk_transient_copy(zero, sizeof zero, 256);
    return (VkDescriptorBufferInfo){s.buf, s.off, sizeof zero};
}

void gmem_reset(void) {
    if (gmem_buf) {
        if (gmem_alloc) vmaDestroyBuffer(g_vk.vma, gmem_buf, gmem_alloc);
        else { vkDestroyBuffer(g_vk.dev, gmem_buf, NULL); vkFreeMemory(g_vk.dev, gmem_mem, NULL); }
        gmem_buf = VK_NULL_HANDLE;
        gmem_mem = VK_NULL_HANDLE;
        gmem_alloc = NULL;
    }
    gmem_size = gmem_tried = 0;
}

bool gmem_is_mirror(void) {
    return gmem_alloc != NULL;
}

void gmem_interest(uint64_t addr, uint64_t size) {
    if (gmem_alloc) nvn_heap_mirror_interest(addr, size);
}

bool gmem_ensure_size(uint64_t addr, uint64_t size) {
    if (addr + size > HEAP_BASE + gmem_size && kernel_heap_size() != gmem_tried) {
        gmem_tried = kernel_heap_size();
        gmem_info();
    }
    return gmem_buf && (addr + size <= HEAP_BASE + gmem_size);
}
// [extra]
