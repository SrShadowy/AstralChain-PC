// [extra]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <pthread.h>
#include <SDL3/SDL_timer.h>
#include "../../../../settings.h"
#include "../../exec_internal.h"
#include "../../../renderer.h"
#include "submission_internal.h"

static char stats_text[2048];

// (LARECOMP_VK_PROF) called once per presented frame: frames over 50 ms are logged with where their time went.
// present_ns: time in backend_present's acquire/blit/present.
void vk_prof_frame(uint64_t frame, uint64_t present_ns) {
    if (!prof) return;
    static uint64_t last_t, l_exec, l_wait, l_draws, l_subs, l_up, l_built;
    uint64_t now = SDL_GetTicksNS();
    if (last_t && now - last_t > 50000000) {
        uint64_t dt = now - last_t, ex = cum_exec - l_exec, wt = cum_wait - l_wait;
        LOG("[hitch] frame %llu at %.3f s: %.0f ms = backend exec %.1f (of which GPU waits %.1f, texture uploads %.1f, waits for "
            "pipelines %.1f) + present %.1f + other %.1f; %llu draws, %llu submits, compiles %d pending\n", (unsigned long long)frame,
            now / 1e9, dt / 1e6, ex / 1e6, wt / 1e6, (cum_up_ns - l_up) / 1e6, (cum_built_ns - l_built) / 1e6, present_ns / 1e6,
            ((double)dt - ex - present_ns) / 1e6, (unsigned long long)(cum_draws - l_draws), (unsigned long long)(cum_submits - l_subs),
            atomic_load(&stat_compiles));
    }
    last_t = now; l_exec = cum_exec; l_wait = cum_wait; l_draws = cum_draws; l_subs = cum_submits; l_up = cum_up_ns;
    l_built = cum_built_ns;
}

const char *renderer_stats(void) {
    static int dumps;
    if (dumps++ < 3) {
        LOG("[vk] command counts:");
        for (int i = 0; i < CMD_COUNT; i++) if (op_counts[i]) LOG(" %d:%llu", i, (unsigned long long)op_counts[i]);
        LOG("\n");
    }
    snprintf(stats_text, sizeof stats_text, // [extra]
             "%llu draws, %llu skipped, %llu passes, %llu dispatches, %llu flushes, wait %.0fms, compile %d, fbcopy %llu, upload %.1fMB [vtx:%.1f,cbuf:%.1f]", // [extra]
             (unsigned long long)stat_draws, (unsigned long long)stat_skipped, (unsigned long long)stat_passes,
             (unsigned long long)stat_dispatches, (unsigned long long)stat_flushes, (double)stat_wait_ns / 1e6,
             atomic_exchange(&stat_compiles, 0), (unsigned long long)stat_feedback_copies, (double)stat_upload_bytes / 1048576.0, // [extra]
             (double)stat_up_vtx / 1048576.0, (double)stat_up_cbuf / 1048576.0);
    stat_feedback_copies = stat_upload_bytes = stat_up_vtx = stat_up_cbuf = 0; // [extra]
    {
        unsigned long long tr = atomic_exchange(&build_ns_translate, 0), sp = atomic_exchange(&build_ns_spirv, 0),
                           pi = atomic_exchange(&build_ns_pipe, 0);
        size_t n = strlen(stats_text);
        if (tr || sp || pi)
            snprintf(stats_text + n, sizeof stats_text - n, ", build ms translate %.0f spirv %.0f pipelines %.0f", tr / 1e6, sp / 1e6, pi / 1e6);
        unsigned long long ph = atomic_exchange(&build_pipe_hits, 0), pm = atomic_exchange(&build_pipe_misses, 0),
                           pmn = atomic_exchange(&build_ns_pipe_miss, 0);
        n = strlen(stats_text);
        if (ph || pm) snprintf(stats_text + n, sizeof stats_text - n, " (cache hits %llu, misses %llu in %.0f ms)", ph, pm, pmn / 1e6);
        n = strlen(stats_text);
        if (pwarm_queued || pwarm_ready || pwarm_late || pwarm_rejected)
            snprintf(stats_text + n, sizeof stats_text - n, ", pipeline warm-up queued %llu ready %llu late %llu rejected %llu",
                     pwarm_queued, pwarm_ready, pwarm_late, pwarm_rejected);
        pwarm_queued = pwarm_ready = pwarm_late = pwarm_rejected = 0;
        unsigned long long sb = atomic_exchange(&gpl_stage_built, 0), sh = atomic_exchange(&gpl_stage_hits, 0),
                           sn = atomic_exchange(&gpl_stage_ns, 0), lk = atomic_exchange(&gpl_linked, 0),
                           ln = atomic_exchange(&gpl_link_ns, 0), ob = atomic_exchange(&gpl_opt_built, 0);
        n = strlen(stats_text);
        if (sb || sh || lk || ob)
            snprintf(stats_text + n, sizeof stats_text - n, ", shader libraries built %llu in %.0f ms (reused %llu), linked %llu in %.1f ms, optimized %llu",
                     sb, sn / 1e6, sh, lk, ln / 1e6, ob);
        n = strlen(stats_text);
        if (warm_stat_queued || warm_stat_ready || warm_stat_late)
            snprintf(stats_text + n, sizeof stats_text - n, ", warm-up queued %llu ready %llu late %llu", (unsigned long long)warm_stat_queued,
                     (unsigned long long)warm_stat_ready, (unsigned long long)warm_stat_late);
        warm_stat_queued = warm_stat_ready = warm_stat_late = 0;
    }
    {   // the game's video memory (VMA's allocations in device-local heaps), e.g. for LARECOMP_BC7 comparisons
        const VkPhysicalDeviceMemoryProperties *mp;
        vmaGetMemoryProperties(g_vk.vma, &mp);
        VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
        vmaGetHeapBudgets(g_vk.vma, budgets);
        uint64_t used = 0, budget = 0; // [extra]
        for (uint32_t i = 0; i < mp->memoryHeapCount; i++) {
            if (mp->memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                used += budgets[i].statistics.allocationBytes;
                budget += budgets[i].budget;
            }
        } // [extra]
        size_t n = strlen(stats_text);
        if (budget > 0) { // [extra]
            snprintf(stats_text + n, sizeof stats_text - n, ", video memory %.0fMB / %.0fMB (%.0f%%)",
                     used / 1048576.0, budget / 1048576.0, (used * 100.0) / budget);
        } else {
            snprintf(stats_text + n, sizeof stats_text - n, ", video memory %.0fMB", used / 1048576.0);
        } // [extra]
        extern void vk_bc7_cache_stats(uint64_t *hits, uint64_t *misses);
        uint64_t bh, bm;
        vk_bc7_cache_stats(&bh, &bm);
        n = strlen(stats_text);
        if (bh || bm) snprintf(stats_text + n, sizeof stats_text - n, " (BC7 precomputed: %llu hits, %llu misses)",
                               (unsigned long long)bh, (unsigned long long)bm);
    }
    {
        size_t n = strlen(stats_text);
        snprintf(stats_text + n, sizeof stats_text - n, ", mirror in %.1fMB, write-back %.1fMB in %llu (%llu faults, %d lazy)",
                 (double)stat_mirror_bytes / 1048576.0, (double)stat_wb_bytes / 1048576.0, (unsigned long long)stat_wb_ranges,
                 (unsigned long long)stat_wb_faults, lazy_n);
        stat_mirror_bytes = stat_wb_bytes = stat_wb_ranges = stat_wb_faults = 0;
        for (int k = 0; k < 3; k++) {  // the three most frequent skip sites (exec.c lines)
            int best = 0;
            for (int l = 1; l < 8192; l++) if (skip_lines[l] > skip_lines[best]) best = l;
            if (!skip_lines[best]) break;
            n = strlen(stats_text);
            snprintf(stats_text + n, sizeof stats_text - n, "%s skip@%d %u", k ? "," : ";", best, skip_lines[best]);
            skip_lines[best] = 0;
        }
        memset(skip_lines, 0, sizeof skip_lines);
    }
    {
        extern void vk_alias_stats(void);
        vk_alias_stats();
    }
    if (prof) {
        prof_up = prof_up_ns;
        prof_up_ns = 0;
        size_t n = strlen(stats_text);
        snprintf(stats_text + n, sizeof stats_text - n, " | gpu %.0f cpu exec %.0f draw %.0f (pre %.0f key %.0f dyn %.0f sets %.0f tex %.0f (upload %.0f) vtx %.0f) submit %.0f ms",
                 prof_gpu / 1e6, prof_exec / 1e6, prof_draw / 1e6, prof_pre / 1e6, prof_key / 1e6, prof_dyn / 1e6, prof_sets / 1e6,
                 prof_tex / 1e6, prof_up / 1e6, prof_vtx / 1e6, prof_submit / 1e6);
        extern uint64_t prof_tex_checks, prof_tex_samples, prof_tex_full, prof_tex_full_bytes, prof_tex_dyn;
        n = strlen(stats_text);
        snprintf(stats_text + n, sizeof stats_text - n, " | tex checks %llu samples %llu full %llu (%.1f MB, dynamic %llu)",
                 (unsigned long long)prof_tex_checks, (unsigned long long)prof_tex_samples, (unsigned long long)prof_tex_full,
                 prof_tex_full_bytes / 1048576.0, (unsigned long long)prof_tex_dyn);
        n = strlen(stats_text);
        snprintf(stats_text + n, sizeof stats_text - n, " | pass ends:");
        for (int l = 0; l < 4096; l++)
            if (prof_end_line[l] > 20) { n = strlen(stats_text); snprintf(stats_text + n, sizeof stats_text - n, " %d:%u", l, prof_end_line[l]); }
        memset(prof_end_line, 0, sizeof prof_end_line);
        n = strlen(stats_text);
        snprintf(stats_text + n, sizeof stats_text - n, " | views created %llu | counters gpu %llu | pace %s, %llu frames over", (unsigned long long)prof_views_created,
                 (unsigned long long)stat_counters_gpu, pace_slow ? "30" : "60", (unsigned long long)stat_pace_over);
        stat_counters_gpu = stat_pace_over = 0;
        {
            extern uint64_t prof_vc_hits, prof_vc_bypass, prof_vc_copied, prof_vc_queries, prof_vc_scan_pages, prof_vc_scan_ns;
            n = strlen(stats_text);
            snprintf(stats_text + n, sizeof stats_text - n,  " | vcache %llu hit %llu in place %.1f MB copied, %llu queries %.2f ms, %llu pages written",
                     (unsigned long long)prof_vc_hits, (unsigned long long)prof_vc_bypass, prof_vc_copied / 1048576.0,
                     (unsigned long long)prof_vc_queries, prof_vc_scan_ns / 1e6, (unsigned long long)prof_vc_scan_pages);
            prof_vc_hits = prof_vc_bypass = prof_vc_copied = prof_vc_queries = prof_vc_scan_pages = prof_vc_scan_ns = 0;
            extern uint64_t prof_tex_watch_skips, prof_tex_unknown, prof_tex_warmed;
            n = strlen(stats_text);
            snprintf(stats_text + n, sizeof stats_text - n, " | tex unwritten %llu untracked %llu warmed %llu", (unsigned long long)prof_tex_watch_skips,
                     (unsigned long long)prof_tex_unknown, (unsigned long long)prof_tex_warmed);
            prof_tex_watch_skips = prof_tex_unknown = prof_tex_warmed = 0;
            extern uint64_t prof_ww_calls, prof_ww_qpc, prof_ww_pages, prof_ww_mirror, prof_ww_clean, prof_ww_direct, prof_ww_span;
            n = strlen(stats_text);
            snprintf(stats_text + n, sizeof stats_text - n, " | write watch %llu calls %.1f ms %.0f MB (textures %llu + spans %llu, mirror %llu, clean %llu)",
                     (unsigned long long)prof_ww_calls, prof_ww_qpc / 1e6, prof_ww_pages / 256.0, (unsigned long long)prof_ww_direct,
                     (unsigned long long)prof_ww_span, (unsigned long long)prof_ww_mirror, (unsigned long long)prof_ww_clean);
            prof_ww_calls = prof_ww_qpc = prof_ww_pages = prof_ww_mirror = prof_ww_clean = prof_ww_direct = prof_ww_span = 0;
            extern uint64_t nvn_async_drains, nvn_async_waits;  // (objects.c, AC_ASYNC_SUBMIT)
            n = strlen(stats_text);
            snprintf(stats_text + n, sizeof stats_text - n, " | async drains %llu fence waits %llu", (unsigned long long)nvn_async_drains,
                     (unsigned long long)nvn_async_waits);
            nvn_async_drains = nvn_async_waits = 0;
        }
        pthread_mutex_lock(&wait_tab_lock);
        for (int i = 0; i < 12 && wait_tab[i].why; i++) {
            n = strlen(stats_text);
            snprintf(stats_text + n, sizeof stats_text - n, "%s%s %llux %.0f ms", i ? "," : " | waits:", wait_tab[i].why,
                     (unsigned long long)wait_tab[i].n, wait_tab[i].ns / 1e6);
        }
        memset(wait_tab, 0, sizeof wait_tab);
        pthread_mutex_unlock(&wait_tab_lock);
        prof_views_created = 0;
        prof_tex_checks = prof_tex_samples = prof_tex_full = prof_tex_full_bytes = prof_tex_dyn = 0;
        prof_exec = prof_draw = prof_sets = prof_vtx = prof_submit = prof_pre = prof_key = prof_dyn = prof_tex = prof_up = prof_gpu = 0;
    }
    stat_draws = stat_skipped = stat_passes = stat_dispatches = stat_flushes = stat_wait_ns = 0;
    return stats_text;
}
