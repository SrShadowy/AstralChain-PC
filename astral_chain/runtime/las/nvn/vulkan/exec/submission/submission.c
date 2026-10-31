// [extra]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <pthread.h>
#include <SDL3/SDL_timer.h>
#include "../../../../settings.h"
#include "../../platform.h"
#include "../../exec_internal.h"
#include "../../../renderer.h"
#include "submission_internal.h"

// ================================================================== submissions
// A submission = optional upload command buffer + the main one, signalling the timeline with its number.
#define SUBS 8
typedef struct {
    VkCommandBuffer main, upload;
    bool main_begun, upload_begun, ts;  // ts: GPU timestamps written (profiling)
    uint64_t seq;
    VkDescriptorPool pools[16];
    int npools, cur_pool;
} Sub;
static Sub subs[SUBS];
static int cur_sub;
uint64_t submit_seq;  // last submitted // [extra]
static pthread_mutex_t seq_lock = PTHREAD_MUTEX_INITIALIZER;
static VkQueryPool prof_qp;

uint64_t vk_completed_seq(void) {
    uint64_t v = 0;
    vkGetSemaphoreCounterValue(g_vk.dev, g_vk.timeline, &v);
    return v;
}

__thread const char *wait_why; // [extra]
WaitRecord wait_tab[12];
pthread_mutex_t wait_tab_lock = PTHREAD_MUTEX_INITIALIZER;

static void wait_account(uint64_t ns) {
    const char *why = wait_why ? wait_why : "other";
    wait_why = NULL;
    pthread_mutex_lock(&wait_tab_lock);
    for (int i = 0; i < 12; i++)
        if (!wait_tab[i].why || wait_tab[i].why == why) { wait_tab[i].why = why; wait_tab[i].ns += ns; wait_tab[i].n++; break; }
    pthread_mutex_unlock(&wait_tab_lock);
}

static void cq_resolve_done(uint64_t done);

void vk_wait_seq(uint64_t seq) {
    if (!seq) { wait_why = NULL; return; }
    VkSemaphoreWaitInfo wi = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO, .semaphoreCount = 1, .pSemaphores = &g_vk.timeline,
                              .pValues = &seq};
    uint64_t t0 = SDL_GetTicksNS();
    VkResult r = vkWaitSemaphores(g_vk.dev, &wi, 10ull * 1000000000ull);
    stat_wait_ns += SDL_GetTicksNS() - t0;
    cum_wait += SDL_GetTicksNS() - t0;
    wait_account(SDL_GetTicksNS() - t0);
    if (r == VK_TIMEOUT) LOG("[vk] GPU did not finish submission %llu within 10 s (device lost?)\n", (unsigned long long)seq);
    else if (r == VK_SUCCESS) { cq_resolve_done(seq); gmem_writeback_apply(vk_completed_seq()); }  // (counters, mirror write-backs)
    else {
        extern void vk_devlost_dump(void);
        vk_devlost_dump();
        vk_device_fault_report();
        FATAL("vkWaitSemaphores: %d (device lost)\n", r);
    }
}

uint64_t vk_pending_seq(void) { return submit_seq + 1; }

// GPU hang safety stop: when submitted work makes no progress (the timeline value doesn't advance) for
// AC_GPU_HANG_MS (default 1200; 0 = off), the process exits before Windows' 2 s TDR resets the GPU.
static _Atomic uint64_t hang_submitted;
static void *hang_watch(void *arg) {
    int64_t ms = (int64_t)(intptr_t)arg;
    uint64_t last = 0, since = SDL_GetTicksNS();
    for (;;) {
        SDL_Delay(50);
        uint64_t done = vk_completed_seq(), now = SDL_GetTicksNS();
        if (done >= atomic_load(&hang_submitted) || done != last) { last = done; since = now; continue; }
        if (now - since < (uint64_t)ms * 1000000ull) continue;
        LOG("[vk] SAFETY STOP: GPU made no progress for %lld ms (submission %llu pending, %llu done); exiting before a "
            "driver reset\n", (long long)ms, (unsigned long long)atomic_load(&hang_submitted), (unsigned long long)done);
        fflush(NULL);
        _exit(86);
    }
    return NULL;
}

static void hang_watch_submitted(uint64_t seq) {
    static int started;
    atomic_store(&hang_submitted, seq);
    if (started) return;
    started = 1;
    int64_t ms = getenv("AC_GPU_HANG_MS") ? atoll(getenv("AC_GPU_HANG_MS")) : 1200;
    LOG("[vk] GPU hang stop after %lld ms%s\n", (long long)ms, ms > 0 ? "" : " (off)");
    pthread_t t;
    if (ms > 0 && pthread_create(&t, NULL, hang_watch, (void *)(intptr_t)ms) == 0) pthread_detach(t);
}

// ---- the game's GPU timing counters (nvnCommandBufferReportCounter, two per frame)
#define CQ_MAX 64
typedef struct { VkQueryPool pool; uint32_t n; uint64_t seq; uint64_t *out[CQ_MAX]; } CounterQ;
static CounterQ cq[SUBS];
static pthread_mutex_t cq_lock = PTHREAD_MUTEX_INITIALIZER;
static int64_t cq_offset;  // host ns - GPU ns (the smallest seen)
static uint64_t ns_to_gpu_ticks(uint64_t ns) { return (uint64_t)((unsigned __int128)ns * 384 / 625); }
static bool cq_calibrated;
static int counter_mode = -1;  // 0 cpu, 1 frozen, 2 gpu
uint64_t stat_counters_gpu;

static int vk_counter_mode(void) {
    if (counter_mode < 0) {
        const char *e = getenv("LARECOMP_VK_COUNTER");
        counter_mode = e && !strcmp(e, "cpu") ? 0 : e && !strcmp(e, "frozen") ? 1 : e && !strcmp(e, "legacy") ? 3 : 2;
        if (counter_mode == 2 && !g_vk.props.limits.timestampComputeAndGraphics) counter_mode = 0;
    }
    return counter_mode;
}

// ---- frame pacing: the game's own 30 fps mode when we can't hold 60
#define PACE_SLOW_NS 20000000ull
extern __thread uint64_t g_blocked_ns;  // kernel.c: this thread's time blocked in waits / sleeps
bool pace_slow;
uint64_t stat_pace_over;

void vk_pace_frame(void) {
    static int on = -1;
    if (on < 0) { const char *e = getenv("LARECOMP_PACE"); on = e && atoi(e) != 0; }
    static uint64_t last, last_wait;
    static uint8_t over[60];
    static int idx, nover, calm;
    static double avg;  // busy ms, moving average (~20 frames)
    uint64_t now = plat_now_ns(), w = g_blocked_ns;
    uint64_t p = now - last, wt = w - last_wait;
    bool first = !last;
    last = now, last_wait = w;
    if (first || p > 100000000ull) return;  // (loading, a hitch: not a pace)
    double busy = (p > wt ? p - wt : 0) / 1e6;
    avg = avg ? avg + (busy - avg) * 0.05 : busy;
    static int log = -1;  // LARECOMP_LOG_PACE=1: each frame's period, vsync wait and busy time
    if (log < 0) log = getenv("LARECOMP_LOG_PACE") != NULL;
    if (log) LOG("[pace] period %.2f blocked %.2f busy %.2f ms\n", p / 1e6, wt / 1e6, busy);
    int o = busy > 16.2;
    stat_pace_over += o;
    nover += o - over[idx];
    over[idx] = (uint8_t)o;
    idx = (idx + 1) % 60;
    if (!on) return;  // (measured and counted all the same: LARECOMP_LOG_PACE, the stats)
    if (!pace_slow && nover >= 12) {
        pace_slow = true, calm = 0;
        LOG("[pace] frames don't fit 60 fps (%d of the last 60 over 16.2 ms busy): the game's 30 fps mode\n", nover);
    } else if (pace_slow) {
        calm = avg < 13.0 && nover <= 2 ? calm + 1 : 0;
        if (calm >= 120) {
            pace_slow = false, nover = 0;
            memset(over, 0, sizeof over);
            LOG("[pace] 2 s with headroom: back to the game's own choice\n");
        }
    }
}

static uint64_t pace_end_counter(const uint64_t *start_entry, const uint64_t *entry, uint64_t value) {
    if (!pace_slow || !start_entry || (const uint8_t *)entry - (const uint8_t *)start_entry != 16) return value;
    uint64_t min = start_entry[1] + ns_to_gpu_ticks(PACE_SLOW_NS);
    return value < min ? min : value;
}

static void cq_resolve(int i) {  // (cq_lock held) submission slot i is done: its counters get their GPU times
    CounterQ *q = &cq[i];
    if (!q->n) return;
    uint64_t t[CQ_MAX];
    if (vkGetQueryPoolResults(g_vk.dev, q->pool, 0, q->n, sizeof t, t, 8, VK_QUERY_RESULT_64_BIT) == VK_SUCCESS) {
        double per = g_vk.props.limits.timestampPeriod;
        int64_t cand = (int64_t)plat_now_ns() - (int64_t)((double)t[q->n - 1] * per);
        if (!cq_calibrated || cand < cq_offset) cq_offset = cand, cq_calibrated = true;
        static int log = -1;  // LARECOMP_LOG_COUNTER=1: the host (provisional) and GPU times of each counter
        if (log < 0) log = getenv("LARECOMP_LOG_COUNTER") != NULL;
        for (uint32_t k = 0; k < q->n; k++) {
            uint64_t gpu = ns_to_gpu_ticks((uint64_t)((int64_t)((double)t[k] * per) + cq_offset));
            if (log) LOG("[counter] seq %llu #%u at %llx host %.3f gpu %.3f ms\n", (unsigned long long)q->seq, k,
                         (unsigned long long)(uintptr_t)q->out[k], q->out[k][1] * 625.0 / 384 / 1e6, gpu * 625.0 / 384 / 1e6);
            q->out[k][1] = pace_end_counter(k ? q->out[k - 1] : NULL, q->out[k], gpu);
        }
        stat_counters_gpu += q->n;
    }
    q->n = 0;
}

static void oq_resolve_done(uint64_t done);
static void cq_resolve_done(uint64_t done) {  // every slot whose submission is done
    pthread_mutex_lock(&cq_lock);
    for (int i = 0; i < SUBS; i++) if (cq[i].n && cq[i].seq <= done) cq_resolve(i);
    oq_resolve_done(done);
    pthread_mutex_unlock(&cq_lock);
}

// ---- samples-passed counters (nvnCommandBufferResetCounter / ReportCounter type 1): occlusion queries
#define OQ_MAX 256
#define OQ_REPS 64
typedef struct { uint64_t *out; uint32_t first, count; float area; } OccRep;
typedef struct { VkQueryPool pool; uint32_t n, nrep; OccRep rep[OQ_REPS]; } OccQ;
static OccQ oq[SUBS];
static int occ_sub = -1;  // the slot of the open count (reset seen, not yet reported); -1 none
static uint32_t occ_first;
static uint64_t stat_occ;

static void oq_resolve(int i) {  // (cq_lock held) slot i's submission is done
    OccQ *q = &oq[i];
    if (q->nrep && q->n) {
        static uint64_t res[OQ_MAX];
        static int log = -1;  // LARECOMP_LOG_OCC=1: each report's samples
        if (log < 0) log = getenv("LARECOMP_LOG_OCC") != NULL;
        if (vkGetQueryPoolResults(g_vk.dev, q->pool, 0, q->n, sizeof res, res, 8, VK_QUERY_RESULT_64_BIT) == VK_SUCCESS)
            for (uint32_t k = 0; k < q->nrep; k++) {
                OccRep *r = &q->rep[k];
                uint64_t sum = 0;
                for (uint32_t j = 0; j < r->count; j++) sum += res[r->first + j];
                r->out[0] = (uint64_t)((double)sum / r->area + 0.5);
                if (log) LOG("[occ] report %llx: %u draws, %llu samples (%llu at guest resolution)\n", (unsigned long long)(uintptr_t)r->out,
                             r->count, (unsigned long long)sum, (unsigned long long)r->out[0]);
            }
    }
    q->n = q->nrep = 0;
}

static void oq_resolve_done(uint64_t done) {  // (cq_lock held)
    for (int i = 0; i < SUBS; i++) if (oq[i].nrep && subs[i].seq <= done) oq_resolve(i);
}

void backend_counters_poll(void) { cq_resolve_done(vk_completed_seq()); }

void oq_draw(VkCommandBuffer cb, bool end) {  // around a draw while a count is open // [extra]
    if (occ_sub != cur_sub) return;
    OccQ *q = &oq[cur_sub];
    if (!q->pool || q->n >= OQ_MAX) return;
    if (!end) vkCmdBeginQuery(cb, q->pool, q->n, g_vk.occ_precise ? VK_QUERY_CONTROL_PRECISE_BIT : 0);
    else vkCmdEndQuery(cb, q->pool, q->n++);
}

static Sub *sub(void) {
    Sub *s = &subs[cur_sub];
    if (s->seq != submit_seq + 1) {  // slot reused: its previous submission must be done
        wait_why = "ring slot";
        vk_wait_seq(s->seq);
        cq_resolve_done(s->seq);
        if (s->ts) {  // (profiling) GPU time of that submission
            uint64_t t[2];
            if (vkGetQueryPoolResults(g_vk.dev, prof_qp, (uint32_t)cur_sub * 2, 2, sizeof t, t, 8, VK_QUERY_RESULT_64_BIT) == VK_SUCCESS && t[1] > t[0])
                prof_gpu += (uint64_t)((double)(t[1] - t[0]) * g_vk.props.limits.timestampPeriod);
            s->ts = false;
        }
        for (int i = 0; i < s->npools; i++) vkResetDescriptorPool(g_vk.dev, s->pools[i], 0);
        s->cur_pool = 0;
        s->seq = submit_seq + 1;
        s->main_begun = s->upload_begun = false;
    }
    return s;
}

static void begin_cb(VkCommandBuffer *cb, bool *begun) {
    if (*begun) return;
    if (!*cb) {
        VkCommandBufferAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = g_vk.pool,
                                          .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1};
        VK_CHECK(vkAllocateCommandBuffers(g_vk.dev, &ai, cb));
    }
    VkCommandBufferBeginInfo bi = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    VK_CHECK(vkBeginCommandBuffer(*cb, &bi));
    *begun = true;
}

VkCommandBuffer vk_cmd(void) {
    Sub *s = sub();
    if (!s->main_begun) {
        begin_cb(&s->main, &s->main_begun);
        ss_forget();  // (a new command buffer starts with no state)
        if (vk_counter_mode() == 2) {  // this slot's timing counter queries (reset outside any render pass)
            CounterQ *q = &cq[cur_sub];
            if (!q->pool) {
                VkQueryPoolCreateInfo qi = {.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, .queryType = VK_QUERY_TYPE_TIMESTAMP,
                                            .queryCount = CQ_MAX};
                if (vkCreateQueryPool(g_vk.dev, &qi, NULL, &q->pool) != VK_SUCCESS) q->pool = VK_NULL_HANDLE;
            }
            pthread_mutex_lock(&cq_lock);
            q->n = 0;  // (resolved when the slot was freed: sub())
            q->seq = s->seq;
            pthread_mutex_unlock(&cq_lock);
            if (q->pool) vkCmdResetQueryPool(s->main, q->pool, 0, CQ_MAX);
        }
        {   // this slot's occlusion queries (reset outside any render pass)
            OccQ *q = &oq[cur_sub];
            if (!q->pool) {
                VkQueryPoolCreateInfo qi = {.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, .queryType = VK_QUERY_TYPE_OCCLUSION,
                                            .queryCount = OQ_MAX};
                if (vkCreateQueryPool(g_vk.dev, &qi, NULL, &q->pool) != VK_SUCCESS) q->pool = VK_NULL_HANDLE;
            }
            pthread_mutex_lock(&cq_lock);
            q->n = q->nrep = 0;  // (resolved when the slot was freed: sub())
            pthread_mutex_unlock(&cq_lock);
            if (occ_sub == cur_sub) occ_sub = -1;  // (a count across submissions: dropped)
            if (q->pool) vkCmdResetQueryPool(s->main, q->pool, 0, OQ_MAX);
        }
        gp_mark(s->main, "(submission start; includes GPU idle before it)");
        if (prof && prof_qp) {
            vkCmdResetQueryPool(s->main, prof_qp, (uint32_t)cur_sub * 2, 2);
            vkCmdWriteTimestamp2(s->main, VK_PIPELINE_STAGE_2_NONE, prof_qp, (uint32_t)cur_sub * 2);
            s->ts = true;
        }
    }
    return s->main;
}

VkCommandBuffer vk_upload_cmd(void) {
    Sub *s = sub();
    if (!s->upload_begun) {
        begin_cb(&s->upload, &s->upload_begun);
        vk_full_barrier(s->upload);
    }
    return s->upload;
}

void vk_full_barrier(VkCommandBuffer cb) {
    VkMemoryBarrier2 mb = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2, .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                           .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT, .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                           .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT};
    VkDependencyInfo di = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &mb};
    vkCmdPipelineBarrier2(cb, &di);
}

// ---- LARECOMP_GPUPROF_AT=<frame>: GPU time per pass / dispatch / copy of that frame, logged as "[gpuprof]".
bool g_gpuprof_frame;
enum { GP_MAX = 4096 };
static VkQueryPool gp_qp;
static int gp_n;
static char gp_label[GP_MAX][112];
static uint32_t gp_draws[GP_MAX];

static char dl_ring[64][128];
static unsigned dl_n;
int dl_on = -1; // [extra]
void dl_note(const char *fmt, ...) { // [extra]
    if (dl_on < 0) dl_on = getenv("LARECOMP_VK_DEVLOST_TRACE") ? atoi(getenv("LARECOMP_VK_DEVLOST_TRACE")) : 0;
    if (!dl_on) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(dl_ring[dl_n++ & 63], sizeof dl_ring[0], fmt, ap);
    va_end(ap);
}
void vk_dl_note_upload(void *img, uint32_t w, uint32_t h, uint32_t lv) { dl_note("  upload %p %ux%u lv%u", img, w, h, lv); }
void vk_devlost_dump(void) {
    if (!dl_on) return;
    LOG("[vk] last GPU operations before the device loss (oldest first):\n");
    for (unsigned i = dl_n > 64 ? dl_n - 64 : 0; i < dl_n; i++) LOG("[vk]   %s\n", dl_ring[i & 63]);
}
void gp_mark(VkCommandBuffer cb, const char *fmt, ...) { // [extra]
    if (!g_gpuprof_frame || gp_n >= GP_MAX - 1) return;
    if (!gp_qp) {
        VkQueryPoolCreateInfo qi = {.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = GP_MAX};
        if (vkCreateQueryPool(g_vk.dev, &qi, NULL, &gp_qp) != VK_SUCCESS) { g_gpuprof_frame = false; return; }
    }
    if (!gp_n) vkCmdResetQueryPool(cb, gp_qp, 0, GP_MAX);
    vkCmdWriteTimestamp2(cb, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, gp_qp, (uint32_t)gp_n);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gp_label[gp_n], sizeof gp_label[0], fmt, ap);
    va_end(ap);
    gp_draws[gp_n] = 0;
    gp_n++;
}
void gp_record_draw(uint64_t fs_hash) { // [extra]
    if (g_gpuprof_frame && gp_n) { // [extra]
        if (!gp_draws[gp_n - 1]) { // [extra]
            size_t l = strlen(gp_label[gp_n - 1]); // [extra]
            snprintf(gp_label[gp_n - 1] + l, sizeof gp_label[0] - l, ", first fs %016llx", (unsigned long long)fs_hash); // [extra]
        } // [extra]
        gp_draws[gp_n - 1]++; // [extra]
    } // [extra]
} // [extra]

void vk_gpuprof_report(void) {
    if (!gp_n) return;
    renderer_flush();
    static uint64_t t[GP_MAX];
    if (vkGetQueryPoolResults(g_vk.dev, gp_qp, 0, (uint32_t)gp_n, sizeof t, t, 8, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT) == VK_SUCCESS) {
        double per = g_vk.props.limits.timestampPeriod / 1e6, total = (t[gp_n - 1] - t[0]) * per;
        LOG("[gpuprof] %d marks, %.2f ms first to last (rows: the work after each mark, until the next)\n", gp_n, total);
        for (int i = 0; i + 1 < gp_n; i++) {
            double ms = (t[i + 1] - t[i]) * per;
            LOG("[gpuprof] %6.3f ms %5.1f%%  %s%s", ms, 100 * ms / (total > 0 ? total : 1), gp_label[i], gp_draws[i] ? "" : "\n");
            if (gp_draws[i]) LOG(", %u draws\n", gp_draws[i]);
        }
    }
    gp_n = 0;
}

// Submit the pending work (if any). wait/signal: binary semaphores for presentation (may be null).
void vk_submit_extra(VkSemaphore wait, VkSemaphore signal, VkCommandBuffer extra) {
    (void)extra;
    vk_end_pass();
    Sub *s = sub();
    if (!s->main_begun && !s->upload_begun && !wait && !signal) return;
    VkCommandBufferSubmitInfo cbs[2];
    int n = 0;
    gmem_sync();
    vc_flush();
    if (s->upload_begun) {
        vk_full_barrier(s->upload);  // uploads complete before anything in the main buffer reads them
        VK_CHECK(vkEndCommandBuffer(s->upload));
        cbs[n++] = (VkCommandBufferSubmitInfo){.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = s->upload};
    }
    vk_cmd();  // (an empty main buffer still carries the semaphores)
    gmem_wb_record(s->main, submit_seq + 1);
    if (s->ts) vkCmdWriteTimestamp2(s->main, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, prof_qp, (uint32_t)cur_sub * 2 + 1);
    VK_CHECK(vkEndCommandBuffer(s->main));
    cbs[n++] = (VkCommandBufferSubmitInfo){.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = s->main};
    VkSemaphoreSubmitInfo sig[2] = {{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, .semaphore = g_vk.timeline,
                                     .value = submit_seq + 1, .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT},
                                    {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, .semaphore = signal,
                                     .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}};
    VkSemaphoreSubmitInfo wt = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, .semaphore = wait,
                                .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo2 si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2, .commandBufferInfoCount = (uint32_t)n, .pCommandBufferInfos = cbs,
                        .signalSemaphoreInfoCount = signal ? 2 : 1, .pSignalSemaphoreInfos = sig,
                        .waitSemaphoreInfoCount = wait ? 1 : 0, .pWaitSemaphoreInfos = &wt};
    {
        PROF_T0();
        dl_note("submit %llu", (unsigned long long)submit_seq + 1);
        VkResult sr = vkQueueSubmit2(g_vk.queue, 1, &si, VK_NULL_HANDLE);
        if (sr == VK_ERROR_DEVICE_LOST) vk_devlost_dump();
        VK_CHECK(sr);
        PROF_ADD(prof_submit);
    }
    pthread_mutex_lock(&seq_lock);
    submit_seq++;
    hang_watch_submitted(submit_seq);
    pthread_mutex_unlock(&seq_lock);
    cur_sub = (cur_sub + 1) % SUBS;
    stat_flushes++;
    cum_submits++;
    ring_retire();
    extern void vk_collect_dead(void);
    vk_collect_dead();
}

void renderer_submit(void) { vk_submit_extra(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE); }

void renderer_flush(void) {
    renderer_submit();
    if (!wait_why) wait_why = "flush";
    vk_wait_seq(submit_seq);
}

void renderer_sync(void) {
    pthread_mutex_lock(&seq_lock);
    uint64_t s = submit_seq;
    pthread_mutex_unlock(&seq_lock);
    wait_why = "QueueFinish / sync all";
    vk_wait_seq(s);
}

uint64_t renderer_fence_seq(void) {
    pthread_mutex_lock(&seq_lock);
    uint64_t q = submit_seq + (subs[cur_sub].main_begun || subs[cur_sub].upload_begun ? 1 : 0);
    pthread_mutex_unlock(&seq_lock);
    return q;
}

uint64_t renderer_wait_target(uint64_t seq) {
    pthread_mutex_lock(&seq_lock);
    uint64_t s = submit_seq;
    pthread_mutex_unlock(&seq_lock);
    return seq > s ? s : seq;
}

int renderer_seq_done(uint64_t seq) {
    uint64_t done = vk_completed_seq();
    gmem_writeback_apply(done);
    return !seq || done >= seq;
}

void renderer_wait_seq(uint64_t seq) {
    if (!seq) return;
    pthread_mutex_lock(&seq_lock);
    uint64_t s = submit_seq;
    pthread_mutex_unlock(&seq_lock);
    wait_why = seq > s ? "SyncWait (fence not submitted yet)" : "SyncWait";
    vk_wait_seq(seq > s ? s : seq);
}

VkDescriptorSet vk_alloc_set(VkDescriptorSetLayout layout) {
    Sub *s = sub();
    for (;;) {
        if (s->cur_pool == s->npools) {
            if (s->npools == 16) FATAL("descriptor pools exhausted\n");
            VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32768}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4096},
                                            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 32768},
                                            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1024}, {VK_DESCRIPTOR_TYPE_SAMPLER, 1024},
                                            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4096}};  // (Astral Chain: SUST)
            VkDescriptorPoolCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 8192,
                                             .poolSizeCount = 6, .pPoolSizes = sizes};
            VK_CHECK(vkCreateDescriptorPool(g_vk.dev, &ci, NULL, &s->pools[s->npools++]));
        }
        VkDescriptorSetAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                          .descriptorPool = s->pools[s->cur_pool], .descriptorSetCount = 1, .pSetLayouts = &layout};
        VkDescriptorSet set;
        VkResult r = vkAllocateDescriptorSets(g_vk.dev, &ai, &set);
        if (r == VK_SUCCESS) return set;
        s->cur_pool++;
    }
}

void handle_reset_counter(uint32_t type) {
    if (type == 1) {  // (type 1: samples passed)
        vk_cmd();  // (first: may begin the buffer, which resets the slot's queries)
        occ_sub = cur_sub;
        occ_first = oq[cur_sub].n;
    }
}

void handle_report_counter(uint32_t type, uint64_t *out) {
    if (type == 1) {  // samples passed: the open count's queries, summed once the submission is done
        if (!out) return;
        out[1] = ns_to_gpu_ticks(plat_now_ns());
        OccQ *q = &oq[cur_sub];
        bool pending = false;
        if (occ_sub == cur_sub && q->n > occ_first) {
            NvnTexture *rt = st.rt.depth ? st.rt.depth : st.rt.ncolor ? st.rt.color[0] : NULL;
            float sx = 1, sy = 1;
            if (rt) renderer_texture_scale(rt, &sx, &sy);
            pthread_mutex_lock(&cq_lock);
            if (q->nrep < OQ_REPS) { q->rep[q->nrep++] = (OccRep){out, occ_first, q->n - occ_first, sx * sy > 0 ? sx * sy : 1}; pending = true; }
            pthread_mutex_unlock(&cq_lock);
            stat_occ++;
        }
        if (!pending) out[0] = 0;
        occ_sub = -1;  // (reset, draws, report: later draws aren't counted)
        return;
    }
    static uint32_t seen;
    if (type < 32 && !(seen & (1u << type))) { seen |= 1u << type; LOG("[nvn] ReportCounter type %u\n", type); }
    int mode = vk_counter_mode();
    static uint64_t frame_seen = ~0ull, frame_ts;
    extern uint64_t vk_frame_counter(void);
    if (mode == 1 && vk_frame_counter() != frame_seen) { frame_seen = vk_frame_counter(); frame_ts = ns_to_gpu_ticks(plat_now_ns()); }
    if (!out) return;
    out[0] = 0;
    out[1] = mode == 1 ? frame_ts : mode == 3 ? SDL_GetTicksNS() : ns_to_gpu_ticks(plat_now_ns());  // (3 legacy: ns, the old bug)
    static uint64_t *prev_out;  // (frame pacing: the pair's start)
    if (mode == 0 || mode == 2) out[1] = pace_end_counter(prev_out, out, out[1]);
    prev_out = out;
    {   // (test) LARECOMP_TEST_COUNTER_DELAY_MS=<ms>: stall the host after a frame's first counter - a slow CPU
        static double delay = -1;
        static uint64_t delayed_frame = ~0ull;
        if (delay < 0) delay = getenv("LARECOMP_TEST_COUNTER_DELAY_MS") ? atof(getenv("LARECOMP_TEST_COUNTER_DELAY_MS")) : 0;
        if (delay > 0 && vk_frame_counter() != delayed_frame) {
            delayed_frame = vk_frame_counter();
            SDL_DelayPrecise((Uint64)(delay * 1e6));
        }
    }
    if (mode == 2 && type == 0) {  // (type 0: timestamp)
        VkCommandBuffer cb = vk_cmd();  // (first: may begin the buffer, which resets the slot's queries)
        CounterQ *q = &cq[cur_sub];
        pthread_mutex_lock(&cq_lock);
        if (q->pool && q->n < CQ_MAX) {
            vkCmdWriteTimestamp2(cb, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, q->pool, q->n);
            q->out[q->n++] = out;
        }
        pthread_mutex_unlock(&cq_lock);
    }
}

void submission_init(void) {
    if (prof) {
        VkQueryPoolCreateInfo qi = {.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = SUBS * 2};
        if (vkCreateQueryPool(g_vk.dev, &qi, NULL, &prof_qp) != VK_SUCCESS) prof_qp = VK_NULL_HANDLE;
    }
}
