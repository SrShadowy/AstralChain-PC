// [extra]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL_timer.h>
#include "kernel.h"
#include "rt.h"
#include "../../exec_internal.h"

// ---- vertex / index cache
// Reading vertex / index data in place from the imported heap crosses PCIe for every vertex fetched on a discrete GPU
// (outdoors at 3x internal resolution on an RTX 3080 Ti: ~0.7-1.9 ms of GPU a frame; more on PCIe 3.0), and copying it
// per draw instead moves about as much onto the CPU. So each range bound for drawing gets a device-local copy, refreshed
// only where the CPU wrote since (the OS's page write tracking, plat_written_pages, asked at submission: vc_flush): one
// sequential GPU copy of the written pages in the submission's upload buffer (which completes before its draws) instead
// of fetches across the bus. Overlapping ranges are separate entries; the per-page generation keeps them all current.
// Pages the GPU writes itself (a compute dispatch's copy-back, gfast_finish) are never cached: draws over them read the
// heap in place as before, and so does everything when the OS gives no write tracking. LARECOMP_VK_VCACHE=0: off.
#define VC_PAGE 4096ull
#define VC_TAB 8192                   // entries (open addressing)
#define VC_MAX_RANGE (64ull << 20)    // larger bound ranges: read in place
#define VC_BUDGET (768ull << 20)      // device-local bytes kept; the least recently used go first
typedef struct { uint64_t lo, hi, used_seq; uint32_t gen, epoch; VkBuffer buf; VmaAllocation mem; bool gpu; } VcEnt;
static VcEnt vc_tab[VC_TAB];
static uint32_t vc_pend[VC_TAB];  // entries the pending submission draws from (vc_flush brings them up to date)
static int vc_npend;
static uint32_t *vc_page_gen;  // per heap page: the generation of its last CPU write seen (vc_gen when observed)
static uint8_t *vc_page_gpu;   // per heap page: written by the GPU
static uint32_t *vc_page_ggen;  // per heap page: the generation of the last recorded GPU write (texture checks)
static uint32_t *vc_page_gframe;  // per heap page: the frame it was last recorded (GPU writes still in flight)
static uint32_t vc_gen = 1, vc_epoch = 1;  // (epoch: bumped when pages turn GPU-written: entries check again)
static uint64_t vc_bytes;
#define VC_DEAD 1024
static struct { VkBuffer b; VmaAllocation m; uint64_t seq; } vc_dead[VC_DEAD];
static int vc_ndead;
static int vc_on = -1;
// (profiler: draws served / read in place, bytes refreshed, write tracking queries, written pages seen, query time)
uint64_t prof_vc_hits, prof_vc_bypass, prof_vc_copied, prof_vc_queries, prof_vc_scan_pages, prof_vc_scan_ns;

static void vc_free_dead(bool all) {
    uint64_t done = vk_completed_seq();
    int k = 0;
    for (int i = 0; i < vc_ndead; i++) {
        if (!all && vc_dead[i].seq > done) { vc_dead[k++] = vc_dead[i]; continue; }
        vmaDestroyBuffer(g_vk.vma, vc_dead[i].b, vc_dead[i].m);
    }
    vc_ndead = k;
}

static bool vc_drop(VcEnt *e) {  // (freed once the submissions that may read it are done)
    if (vc_ndead == VC_DEAD) { vk_wait_seq(vk_pending_seq() - 1); vc_free_dead(false); }
    if (vc_ndead == VC_DEAD) return false;  // (all read by the pending submission: keep it)
    vc_dead[vc_ndead].b = e->buf; vc_dead[vc_ndead].m = e->mem; vc_dead[vc_ndead++].seq = e->used_seq;
    vc_bytes -= e->hi - e->lo;
    memset(e, 0, sizeof *e);
    return true;
}

// The page tables (vc_page_gen / vc_page_gpu) exist whenever the OS gives write tracking: the vertex cache and the
// texture upload checks (vk_pages_changed) share them. Each write tracking query resets the OS bits, so every query
// stamps the pages it saw written with a new generation, and each user compares against the generation it last saw.
static int pt_state = -1;  // 1: tracking, 0: none
static bool pt_init(void) {
    if (pt_state >= 0) return pt_state;
    uint64_t pg = VC_PAGE, one;
    int n = plat_written_pages(HEAP_BASE, VC_PAGE, &one, 1, &pg);  // (is there write tracking?)
    pt_state = n >= 0 && pg == VC_PAGE;
    if (!pt_state) { LOG("[vk] no page write tracking (%d, page %llu)\n", n, (unsigned long long)pg); return false; }
    vc_page_gen = calloc(HEAP_MAX / VC_PAGE, sizeof *vc_page_gen);
    vc_page_gpu = calloc(HEAP_MAX / VC_PAGE, 1);
    vc_page_ggen = calloc(HEAP_MAX / VC_PAGE, sizeof *vc_page_ggen);
    vc_page_gframe = calloc(HEAP_MAX / VC_PAGE, sizeof *vc_page_gframe);
    vc_page_gen[0] = ++vc_gen;  // (the probe took page 0's tracking)
    return true;
}

// Texture upload checks (textures.c): may guest memory [addr, addr + size) have changed since generation `since`?
// 0: no write since, 1: written, -1: unknown (no tracking, outside the heap, or GPU writes recorded in the last
// few frames: they may not have run yet). GPU writes count by the generation they were recorded at: pages a
// storing shader may write were a permanent "unknown" before, which left textures there on a sampled hash only
// (stale blocks in large textures - character faces). *now: the generation to pass next time.
// Batched texture checks. A write tracking query costs ~6 us however small its range (+ ~2.3 ns a page), and a busy
// frame checks ~800 textures, one query each: ~600 ms in 2 s, a third of the render thread, in the street after the
// opening helicopter (33-45 fps there, at any resolution). The ranges checked in the last frame are merged into spans
// (gaps under 8 MB cost less to scan than a query); within one command list (backend_execute) a span is queried once,
// at the first check inside it, and the later checks inside it read the stamps without a system call. A write made
// after that query is seen at the next command list, as a write just after a texture's own query was before; on the
// Switch the game has to finish its writes before it submits. Ranges outside the spans are queried as before.
// LARECOMP_VK_WW_BATCH=0: one query per check.
extern int plat_write_harvest(uint64_t addr, uint64_t size);
extern int plat_written_pages_harvested(uint64_t addr, uint64_t size, uint64_t *pages, int max, uint64_t *page);
enum { WW_REC = 16384, WW_SPAN = 1024, WW_GAP = 2048 * VC_PAGE };
typedef struct { uint64_t lo, hi; } WwRange;
static WwRange ww_rec[WW_REC], ww_span[WW_SPAN];
static int ww_nrec, ww_nspan, ww_on = -1;
static uint32_t ww_span_list[WW_SPAN], ww_list;  // (the command list a span was last queried in)
static bool ww_in_list;
uint64_t prof_ww_direct, prof_ww_span;
static uint64_t ww_frame;
static int ww_cmp(const void *a, const void *b) {
    uint64_t x = ((const WwRange *)a)->lo, y = ((const WwRange *)b)->lo;
    return x < y ? -1 : x > y;
}
static void ww_new_frame(uint64_t frame) {  // spans from the ranges checked in the frame that ended
    ww_frame = frame;
    qsort(ww_rec, (size_t)ww_nrec, sizeof ww_rec[0], ww_cmp);
    ww_nspan = 0;
    for (int i = 0; i < ww_nrec && ww_nspan < WW_SPAN; i++) {
        if (ww_nspan && ww_rec[i].lo <= ww_span[ww_nspan - 1].hi + WW_GAP) {
            if (ww_rec[i].hi > ww_span[ww_nspan - 1].hi) ww_span[ww_nspan - 1].hi = ww_rec[i].hi;
        } else ww_span[ww_nspan++] = ww_rec[i];
    }
    memset(ww_span_list, 0, sizeof ww_span_list);
    ww_nrec = 0;
}
static int ww_written_pages(uint64_t lo, uint64_t hi, uint64_t *pages, int max, uint64_t *pg) {  // (heap offsets)
    if (ww_on < 0) { const char *e = getenv("LARECOMP_VK_WW_BATCH"); ww_on = !e || atoi(e) != 0; }
    if (!ww_on || !ww_in_list) { prof_ww_direct++; return plat_written_pages(HEAP_BASE + lo, hi - lo, pages, max, pg); }
    extern uint64_t vk_frame_counter(void);
    uint64_t frame = vk_frame_counter();
    if (frame != ww_frame) ww_new_frame(frame);
    if (ww_nrec < WW_REC) ww_rec[ww_nrec++] = (WwRange){lo, hi};
    int a = 0, b = ww_nspan;  // (the last span starting at or before lo)
    while (a < b) { int m = (a + b) / 2; if (ww_span[m].lo <= lo) a = m + 1; else b = m; }
    int k = a - 1;
    if (k < 0 || hi > ww_span[k].hi) { prof_ww_direct++; return plat_written_pages(HEAP_BASE + lo, hi - lo, pages, max, pg); }
    if (ww_span_list[k] != ww_list) {
        prof_ww_span++;
        if (plat_write_harvest(HEAP_BASE + ww_span[k].lo, ww_span[k].hi - ww_span[k].lo) < 0)
            return plat_written_pages(HEAP_BASE + lo, hi - lo, pages, max, pg);
        ww_span_list[k] = ww_list;
    }
    return plat_written_pages_harvested(HEAP_BASE + lo, hi - lo, pages, max, pg);
}

void ww_enter_list(void) {
    if (!++ww_list) ww_list = 1;  // (0: never queried)
    ww_in_list = true;
}

void ww_leave_list(void) {
    ww_in_list = false;
}

int vk_pages_changed(uint64_t addr, uint64_t size, uint32_t since, uint32_t *now) {
    if (!size || addr < HEAP_BASE || addr - HEAP_BASE + size > HEAP_MAX || !pt_init()) return -1;
    uint64_t lo = (addr - HEAP_BASE) & ~(VC_PAGE - 1), hi = (addr - HEAP_BASE + size + VC_PAGE - 1) & ~(VC_PAGE - 1);
    static uint64_t pages[4096];
    uint64_t pg = VC_PAGE;
    int n = ww_written_pages(lo, hi, pages, 4096, &pg);
    if (n < 0) return -1;
    uint32_t g = ++vc_gen;
    if (n >= 4096) for (uint64_t p = lo / VC_PAGE; p < hi / VC_PAGE; p++) vc_page_gen[p] = g;
    else for (int k = 0; k < n; k++) vc_page_gen[(pages[k] - HEAP_BASE) / VC_PAGE] = g;
    *now = g;
    int changed = 0;
    extern uint64_t vk_frame_counter(void);
    uint32_t frame = (uint32_t)vk_frame_counter();
    for (uint64_t p = lo / VC_PAGE; p < hi / VC_PAGE; p++) {
        if (vc_page_gpu[p]) {
            if (frame - vc_page_gframe[p] <= 4) return -1;
            if (vc_page_ggen[p] > since) changed = 1;
        }
        if (vc_page_gen[p] > since) changed = 1;
    }
    return changed;
}

// Heap pages [lo, hi) (offsets) written by the GPU: never cached from now on.
static void vc_mark_gpu(uint64_t lo, uint64_t hi) {
    if (!pt_init()) return;
    extern uint64_t vk_frame_counter(void);
    uint32_t g = ++vc_gen, frame = (uint32_t)vk_frame_counter();
    for (uint64_t p = lo / VC_PAGE; p < (hi + VC_PAGE - 1) / VC_PAGE && p < HEAP_MAX / VC_PAGE; p++) {
        if (!vc_page_gpu[p]) { vc_page_gpu[p] = 1; vc_epoch++; }
        vc_page_ggen[p] = g;
        vc_page_gframe[p] = frame;
    }
}

// A storing shader (stage 0 / 1 / 5) may write any of its bound storage buffers.
void vc_mark_sbufs(int stage) {
    for (int i = 0; i < 16; i++) {
        uint64_t a = st.sbuf[stage][i].addr, sz = st.sbuf[stage][i].size;
        if (a >= HEAP_BASE && sz && a - HEAP_BASE < HEAP_MAX) {
            static int big;  // (wide storage bindings turn much of the heap GPU-written: textures there lose page tracking)
            if (sz >= (8u << 20) && big < 16) { big++; LOG("[vk] storage buffer stage %d slot %d: 0x%llx + %.1f MB\n", stage, i, (unsigned long long)a, sz / 1048576.0); }
            vc_mark_gpu(a - HEAP_BASE, a - HEAP_BASE + sz); gmem_wb_note(a, sz);
        }
    }
}

static bool vcache_lookup(uint64_t addr, uint64_t size, VkBuffer *buf, VkDeviceSize *off) {
    if (vc_on < 0) {
        const char *e = getenv("LARECOMP_VK_VCACHE");
        vc_on = !e || atoi(e) != 0;
        if (vc_on && !pt_init()) {
            LOG("[vk] vertex cache off: no page write tracking\n");
            vc_on = 0;
        }
    }
    if (!vc_on || !gmem_buf || size > VC_MAX_RANGE) return false;
    uint64_t lo = (addr - HEAP_BASE) & ~(VC_PAGE - 1), hi = (addr - HEAP_BASE + size + VC_PAGE - 1) & ~(VC_PAGE - 1);
    if (hi > gmem_size) return false;
    uint64_t pend = vk_pending_seq();
    uint64_t h = (lo * 0x9E3779B97F4A7C15ull) ^ (hi * 0xC2B2AE3D27D4EB4Full);
    uint32_t slot = (uint32_t)(h ^ (h >> 32)) & (VC_TAB - 1), free_slot = ~0u, old_slot = ~0u;
    VcEnt *e = NULL;
    for (int probe = 0; probe < 16; probe++, slot = (slot + 1) & (VC_TAB - 1)) {
        VcEnt *c = &vc_tab[slot];
        if (c->buf && c->lo == lo && c->hi == hi) { e = c; break; }
        if (!c->buf && free_slot == ~0u) free_slot = slot;
        if (c->buf && c->used_seq < pend && (old_slot == ~0u || c->used_seq < vc_tab[old_slot].used_seq)) old_slot = slot;
    }
    if (!e) {  // a new range: its own device-local buffer (gen 0: copied whole at the submission)
        if (vc_npend == VC_TAB) return false;
        if (vc_ndead) vc_free_dead(false);
        if (free_slot == ~0u) {
            if (old_slot == ~0u || !vc_drop(&vc_tab[old_slot])) return false;
            free_slot = old_slot;
        }
        while (vc_bytes + (hi - lo) > VC_BUDGET) {  // over budget: the least recently used entry goes
            int lru = -1;
            for (int i = 0; i < VC_TAB; i++)
                if (vc_tab[i].buf && vc_tab[i].used_seq < pend && (lru < 0 || vc_tab[i].used_seq < vc_tab[lru].used_seq)) lru = i;
            if (lru < 0 || !vc_drop(&vc_tab[lru])) return false;
        }
        e = &vc_tab[free_slot];
        VkBufferCreateInfo bi = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = hi - lo,
                                 .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};
        VmaAllocationCreateInfo ai = {.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
        if (vmaCreateBuffer(g_vk.vma, &bi, &ai, &e->buf, &e->mem, NULL) != VK_SUCCESS) { memset(e, 0, sizeof *e); return false; }
        e->lo = lo; e->hi = hi;
        vc_bytes += hi - lo;
    }
    if (e->epoch != vc_epoch) {  // (pages turned GPU-written since: is this range one of them?)
        e->gpu = false;
        for (uint64_t p = lo / VC_PAGE; p < hi / VC_PAGE && !e->gpu; p++) e->gpu = vc_page_gpu[p];
        e->epoch = vc_epoch;
    }
    if (e->gpu) { prof_vc_bypass++; return false; }
    if (e->used_seq != pend) {  // first use in this submission: brought up to date by vc_flush before it runs
        e->used_seq = pend;
        vc_pend[vc_npend++] = (uint32_t)(e - vc_tab);
    }
    prof_vc_hits++;
    *buf = e->buf;
    *off = addr - HEAP_BASE - lo;
    return true;
}

static int vc_pend_cmp(const void *a, const void *b) {
    uint64_t x = vc_tab[*(const uint32_t *)a].lo, y = vc_tab[*(const uint32_t *)b].lo;
    return x < y ? -1 : x > y;
}

// At submission (vk_submit_extra, before the upload buffer closes): the pages the CPU wrote in the ranges this
// submission draws from - one write tracking query per run of nearby ranges (a query costs about as much as scanning
// ~64 pages: ranges closer than that share one) - and their copies into the cache, ahead of the draws.
void vc_flush(void) {
    if (!vc_npend) return;
    qsort(vc_pend, (size_t)vc_npend, sizeof vc_pend[0], vc_pend_cmp);
    uint32_t g = ++vc_gen;
    static uint64_t pages[4096];
    uint64_t t0 = SDL_GetTicksNS();
    for (int i = 0; i < vc_npend;) {
        uint64_t lo = vc_tab[vc_pend[i]].lo, hi = vc_tab[vc_pend[i]].hi;
        int j = i + 1;
        for (; j < vc_npend && vc_tab[vc_pend[j]].lo <= hi + 64 * VC_PAGE; j++)
            if (vc_tab[vc_pend[j]].hi > hi) hi = vc_tab[vc_pend[j]].hi;
        uint64_t pg = VC_PAGE;
        int n = plat_written_pages(HEAP_BASE + lo, hi - lo, pages, 4096, &pg);
        prof_vc_queries++;
        if (n < 0 || n >= 4096) for (uint64_t p = lo / VC_PAGE; p < hi / VC_PAGE; p++) vc_page_gen[p] = g;  // (all)
        else for (int k = 0; k < n; k++) vc_page_gen[(pages[k] - HEAP_BASE) / VC_PAGE] = g;
        if (n > 0) prof_vc_scan_pages += (uint64_t)n;
        i = j;
    }
    prof_vc_scan_ns += SDL_GetTicksNS() - t0;
    VkCommandBuffer cb = vk_upload_cmd();
    for (int i = 0; i < vc_npend; i++) {
        VcEnt *e = &vc_tab[vc_pend[i]];
        VkBufferCopy cp[64];
        int nc = 0;
        for (uint64_t p = e->lo / VC_PAGE; p < e->hi / VC_PAGE; p++) {
            if (e->gen && vc_page_gen[p] <= e->gen) continue;
            uint64_t o = p * VC_PAGE;
            if (nc && cp[nc - 1].srcOffset + cp[nc - 1].size == o) { cp[nc - 1].size += VC_PAGE; continue; }
            if (nc == 64) { nc = 1; cp[0] = (VkBufferCopy){e->lo, 0, e->hi - e->lo}; break; }  // (scattered: the whole range)
            cp[nc++] = (VkBufferCopy){o, o - e->lo, VC_PAGE};
        }
        if (nc) {
            vkCmdCopyBuffer(cb, gmem_buf, e->buf, (uint32_t)nc, cp);
            for (int k = 0; k < nc; k++) prof_vc_copied += cp[k].size;
        }
        e->gen = g;
    }
    vc_npend = 0;
}

// Vertex / index data from the imported guest heap (renderer.m bind_guest reads g_guest_buffer the same way): the
// device-local cache above, else read in place - no per-draw copy, no index scan for the copy size. The game fences its
// CPU writes as on the Switch. Falls back to copies outside the heap or without the import; LARECOMP_VK_COPY_VERTICES=1
// forces copies.
bool guest_direct(uint64_t addr, uint64_t size, VkBuffer *buf, VkDeviceSize *off) {
    static int copy = -1;
    if (copy < 0) {
        const char *e = getenv("LARECOMP_VK_COPY_VERTICES");
#ifndef _WIN32
        copy = e ? (atoi(e) != 0) : 0;
#else
        copy = e ? (atoi(e) != 0) : !pt_init();
#endif
    }
    if (copy || addr < HEAP_BASE) return false;
#ifndef _WIN32 // [extra]
    // Linux lacks page write tracking (pt_init() false), so device mirror (gmem_buf)
    // is never refreshed for CPU-written buffers. UI, HUD, text, dynamic quads (small buffers)
    // must be copied directly from host RAM. Large static meshes stay direct in gmem_buf.
    static uint64_t min_direct = 0;
    if (!min_direct) {
        const char *m = getenv("LARECOMP_VK_DIRECT_MIN_SIZE");
        min_direct = m ? (uint64_t)strtoull(m, NULL, 0) : (512ull * 1024);
    }
    if (size < min_direct) return false;
#endif // [extra]
    if (!gmem_ensure_size(addr, size)) return false;
    gmem_interest(addr, size);
    if (vcache_lookup(addr, size, buf, off)) return true;
    *buf = gmem_buf;
    *off = addr - HEAP_BASE;
    return true;
}

void vc_reset(void) {
    for (int i = 0; i < VC_TAB; i++)
        if (vc_tab[i].buf) vmaDestroyBuffer(g_vk.vma, vc_tab[i].buf, vc_tab[i].mem);
    memset(vc_tab, 0, sizeof vc_tab);
    vc_npend = 0;
    vc_free_dead(true);
    vc_bytes = 0;
    if (vc_page_gpu) memset(vc_page_gpu, 0, HEAP_MAX / VC_PAGE);
    vc_epoch++;
}
// [extra]
