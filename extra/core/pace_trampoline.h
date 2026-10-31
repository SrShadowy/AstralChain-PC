#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sched.h>

/* [extra] Display pacing trampoline with arbitrary AC_FPS support (Linux First).
 *
 * The guest presents on a wall-clock vsync grid built from rt_ticks(). Every
 * constant below that used to be a literal is named here, together with the
 * unit it is expressed in, so the arithmetic in extra_nvn_present_pace() can be
 * read without reverse-engineering magnitudes. */

/* rt_ticks() advances at the Horizon system tick rate (19.2 MHz), not at 1 GHz.
 * The reference conversion lives in host_runtime.c.in (rt_ticks): it rescales
 * the host QPC counter by EXTRA_RT_TICKS_PER_SECOND. */
#define EXTRA_RT_TICKS_PER_SECOND 19200000ULL
#define EXTRA_PACER_SPIN_TICKS    5760ULL /* ~300 us spin window before target tick */

/* Pacing targets, in frames per second. */
#define EXTRA_DEFAULT_TARGET_FPS 60
#define EXTRA_CUTSCENE_FPS       30  /* svc #0xB1 in host_runtime.c.in: cutscenes present at 30 */

/* Sentinels for the one-shot environment lookups. UNREAD means "not cached
 * yet"; UNSET means "variable absent or unusable", meaning the caller keeps the
 * game's own value. They must stay distinct so a real value of 0 (unthrottled)
 * can be told apart from "no override configured". */
#define EXTRA_ENV_UNREAD (-2)
#define EXTRA_ENV_UNSET  (-1)

/* GetEnvironmentVariableA returns the copied length; a value that fills the
 * whole buffer may have been truncated, so it is rejected. */
#define EXTRA_ENV_VALUE_MAX 16

/* Flush both streams every N presents. stdout is block-buffered when piped;
 * flushing more often costs a syscall per frame, less often delays crash logs. */
#define EXTRA_PRESENTS_PER_FLUSH 60

/* Impossible account value, so the first present always logs the pacing state. */
#define EXTRA_LOG_UNSET (-1)

/* The grid advances in whole refreshes, so interval is a refresh count. The
 * guard above already returned for interval <= 0 (unthrottled); the ternary is
 * only a defensive clamp so a present never advances the grid by zero. */
#define EXTRA_MIN_PRESENT_INTERVAL 1ULL

/* If the pacer is late by more than this many refresh periods, it re-anchors on
 * the current tick instead of advancing `last_vsync` step by step. Without it a
 * long stall (level load, driver hitch) is followed by a burst of back-to-back
 * presents that only serves to "pay back" a debt the display never saw. */
#define EXTRA_PACER_RESYNC_PERIODS 4ULL

/* Number of guest thread slots, derived from guest_threads[] in
 * astral_chain/runtime/guest_threads.h so the two cannot drift apart. Slot
 * indices are shared by both. */
#define EXTRA_GUEST_THREAD_SLOTS (sizeof(guest_threads) / sizeof(guest_threads[0]))

/* Cached environment overrides, resolved once per process. */
static inline int extra_env_override(void) {
    static int cached = EXTRA_ENV_UNREAD;
    if (cached == EXTRA_ENV_UNREAD) {
        char v[EXTRA_ENV_VALUE_MAX];
        DWORD n = GetEnvironmentVariableA("AC_PRESENT_INTERVAL", v, sizeof(v));
        cached = (n && n < sizeof(v)) ? atoi(v) : EXTRA_ENV_UNSET;
    }
    return cached;
}

/* AC_FPS, falling back to the legacy AC_TARGET_FPS spelling, then to 60. */
static inline int extra_env_target_fps(void) {
    static int cached = EXTRA_ENV_UNREAD;
    if (cached == EXTRA_ENV_UNREAD) {
        char v[EXTRA_ENV_VALUE_MAX];
        DWORD n = GetEnvironmentVariableA("AC_FPS", v, sizeof(v));
        if (!n) n = GetEnvironmentVariableA("AC_TARGET_FPS", v, sizeof(v));
        cached = (n && n < sizeof(v)) ? atoi(v) : EXTRA_ENV_UNSET;
        if (cached < 0) cached = EXTRA_DEFAULT_TARGET_FPS;
    }
    return cached;
}

/* Counts guest threads other than `self` that are alive and therefore able to
 * make progress while this one waits. Caller holds klock(). */
static inline unsigned extra_other_runnable_threads(unsigned self) {
    unsigned others = 0;
    for (unsigned i = 0; i < EXTRA_GUEST_THREAD_SLOTS; i++) {
        if (i != self && guest_threads[i].status != THREAD_UNUSED &&
            guest_threads[i].status != THREAD_CREATED &&
            guest_threads[i].status != THREAD_EXITED) {
            others++;
        }
    }
    return others;
}

// [extra] Display pacing trampoline with arbitrary AC_FPS support (Linux First)
static inline void extra_nvn_present_pace(int interval) {
    int override = extra_env_override();
    int target_fps = extra_env_target_fps();

    static unsigned presents;
    if (!(++presents % EXTRA_PRESENTS_PER_FLUSH)) {
        fflush(stdout);
        fflush(stderr);
    }

    /* Logged on change only: pacing state is per-session, so a line per frame
     * would drown the telemetry this same stream carries. */
    static int logged_interval = EXTRA_LOG_UNSET, logged_fps = EXTRA_LOG_UNSET;
    if (interval != logged_interval || target_fps != logged_fps) {
        printf("NVN present interval=%d override=%d target_fps=%d\n", interval, override, target_fps);
        logged_interval = interval;
        logged_fps = target_fps;
    }

    if (override >= 0) interval = override;
    if (interval <= 0 || target_fps <= 0) return; /* unthrottled */

    extern int cutscene_mode;
    uint64_t current_fps = (cutscene_mode && target_fps > EXTRA_CUTSCENE_FPS)
                               ? (uint64_t)EXTRA_CUTSCENE_FPS
                               : (uint64_t)target_fps;
    uint64_t period = EXTRA_RT_TICKS_PER_SECOND / current_fps;

    /* The grid origin is re-established whenever the period changes, so a FPS
     * switch or cutscene transition never reuses a stale anchor. */
    static uint64_t origin, last_vsync, prev_period;
    uint64_t now = rt_ticks();
    if (!origin || period != prev_period) {
        origin = now;
        last_vsync = 0;
        prev_period = period;
        return;
    }

    uint64_t step_interval = (uint64_t)(interval > 0 ? interval : EXTRA_MIN_PRESENT_INTERVAL);
    uint64_t due = last_vsync + step_interval;
    if (now - origin > due * period) {
        /* Already past the target refresh: either re-anchor (very late) or
         * accept the current step and return without waiting. */
        last_vsync = now - origin > (due + EXTRA_PACER_RESYNC_PERIODS) * period
                         ? (now - origin) / period
                         : due;
        return;
    }
    uint64_t vsync = (now - origin + period - 1) / period;
    if (vsync < due) vsync = due;
    last_vsync = vsync;
    uint64_t target = origin + vsync * period;
    if (target <= now) return;

    klock();
    threads_initialize();
    unsigned others = extra_other_runnable_threads(current_thread);
    if (!others) {
        /* Sole guest thread: there is nobody to reschedule to, so the wait has
         * to happen here. Hybrid sleep: block with nanosleep for coarse duration,
         * then spin/yield for final sub-millisecond precision. */
        kunlock();
        for (;;) {
            uint64_t cur = rt_ticks();
            if (cur >= target) break;
            uint64_t rem = target - cur;
            if (rem > EXTRA_PACER_SPIN_TICKS) {
                uint64_t sleep_ns = (rem - EXTRA_PACER_SPIN_TICKS) * 1000000000ULL / EXTRA_RT_TICKS_PER_SECOND;
                struct timespec ts = {.tv_sec = (time_t)(sleep_ns / 1000000000ULL),
                                      .tv_nsec = (long)(sleep_ns % 1000000000ULL)};
                nanosleep(&ts, NULL);
            } else {
                Sleep(0);
            }
        }
        return;
    }
    /* Others can run: park this thread on the wake_tick grid instead of
     * spinning, and let the scheduler pick whoever is runnable. */
    guest_threads[current_thread].wake_tick = target;
    guest_threads[current_thread].status = THREAD_SLEEPING;
    (parallel_mode ? 0 : (thread_reschedule = 1));
    kunlock();
}
