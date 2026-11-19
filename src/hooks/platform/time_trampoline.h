#pragma once
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#define EXTRA_TZ_NAME_LEN        32
#define EXTRA_EINVAL_ERRNO       22
#define EXTRA_TIME_ZONE_INVALID  0xFFFFFFFFu
#define EXTRA_SECONDS_PER_MINUTE 60
#define EXTRA_TM_YEAR_BASE       1900

typedef int64_t __time64_t;

typedef struct _SYSTEMTIME {
    uint16_t wYear;
    uint16_t wMonth;
    uint16_t wDayOfWeek;
    uint16_t wDay;
    uint16_t wHour;
    uint16_t wMinute;
    uint16_t wSecond;
    uint16_t wMilliseconds;
} SYSTEMTIME;

typedef struct _TIME_ZONE_INFORMATION {
    int32_t Bias;
    wchar_t StandardName[EXTRA_TZ_NAME_LEN];
    SYSTEMTIME StandardDate;
    int32_t StandardBias;
    wchar_t DaylightName[EXTRA_TZ_NAME_LEN];
    SYSTEMTIME DaylightDate;
    int32_t DaylightBias;
} TIME_ZONE_INFORMATION;

static inline int _localtime64_s(struct tm *result, const __time64_t *timer) {
    if (!result || !timer) return EXTRA_EINVAL_ERRNO;
    time_t t = (time_t)*timer;
    return localtime_r(&t, result) ? 0 : EXTRA_EINVAL_ERRNO;
}

static inline uint32_t GetTimeZoneInformation(TIME_ZONE_INFORMATION *tzi) {
    if (!tzi) return EXTRA_TIME_ZONE_INVALID;
    memset(tzi, 0, sizeof(*tzi));
    time_t t = time(NULL);
    struct tm local;
    localtime_r(&t, &local);
    tzi->Bias = (int32_t)(-local.tm_gmtoff / EXTRA_SECONDS_PER_MINUTE);
    tzi->StandardBias = 0;
    tzi->DaylightBias = 0;
    return 1;
}

static inline void GetLocalTime(SYSTEMTIME *st) {
    if (!st) return;
    time_t t = time(NULL);
    struct tm local;
    localtime_r(&t, &local);
    st->wYear = (uint16_t)(local.tm_year + EXTRA_TM_YEAR_BASE);
    st->wMonth = (uint16_t)(local.tm_mon + 1);
    st->wDayOfWeek = (uint16_t)local.tm_wday;
    st->wDay = (uint16_t)local.tm_mday;
    st->wHour = (uint16_t)local.tm_hour;
    st->wMinute = (uint16_t)local.tm_min;
    st->wSecond = (uint16_t)local.tm_sec;
    st->wMilliseconds = 0;
}

#include <sched.h>
#include <errno.h>

#define EXTRA_TIME_NS_PER_SEC 1000000000ULL
#define EXTRA_SPIN_WINDOW_NS  150000ULL /* 150 us spin window before target deadline */

static inline void extra_cpu_pause(void) {
#if defined(__x86_64__) || defined(_M_X64)
    __builtin_ia32_pause();
#else
    sched_yield();
#endif
}

/* Hybrid precise sleep until monotonic nanosecond target t.
 * Blocks via clock_nanosleep when > 150 us remains, then pauses CPU for final precision. */
static inline void extra_plat_sleep_until_ns(uint64_t t) {
    struct timespec now_ts;
    clock_gettime(CLOCK_MONOTONIC, &now_ts);
    uint64_t now = (uint64_t)now_ts.tv_sec * EXTRA_TIME_NS_PER_SEC + (uint64_t)now_ts.tv_nsec;
    if (now >= t) return;

    if (t - now > EXTRA_SPIN_WINDOW_NS) {
        uint64_t sleep_target = t - EXTRA_SPIN_WINDOW_NS;
        struct timespec ts = {
            .tv_sec = (time_t)(sleep_target / EXTRA_TIME_NS_PER_SEC),
            .tv_nsec = (long)(sleep_target % EXTRA_TIME_NS_PER_SEC)
        };
        while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL) == EINTR) {}
    }

    for (;;) {
        clock_gettime(CLOCK_MONOTONIC, &now_ts);
        now = (uint64_t)now_ts.tv_sec * EXTRA_TIME_NS_PER_SEC + (uint64_t)now_ts.tv_nsec;
        if (now >= t) break;
        extra_cpu_pause();
    }
}

/* Blocking sleep for svc #0xB (SleepThread / yield).
 * Horizon sleeps the calling thread in the scheduler rather than busy-spinning
 * vDSO clock_gettime on sub-millisecond waits. */
static inline void extra_svc_sleep_thread(int64_t ns) {
    if (ns <= 0) {
        sched_yield();
        return;
    }
    struct timespec req;
    req.tv_sec = (time_t)(ns / (int64_t)EXTRA_TIME_NS_PER_SEC);
    req.tv_nsec = (long)(ns % (int64_t)EXTRA_TIME_NS_PER_SEC);
    while (clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &req) == EINTR) {}
}

#define EXTRA_SPIN_FAST_PASSES   16u
#define EXTRA_SPIN_GAP_NS        20000ULL
#define EXTRA_SPIN_HANDOFF_NS    50000ULL
#define EXTRA_SPIN_SLEEP_NS      200000ULL
#define EXTRA_SPIN_SLEEP_MAX_NS  250000ULL

static inline uint64_t extra_monotonic_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * EXTRA_TIME_NS_PER_SEC + (uint64_t)ts.tv_nsec;
}

static inline void extra_spin_gate(uint64_t head) {
    extra_cpu_pause();
    static int s_enabled = -1;
    int on = __atomic_load_n(&s_enabled, __ATOMIC_RELAXED);
    if (on < 0) {
        const char *env = getenv("AC_SPIN");
        on = !(env && env[0] == '0' && env[1] == '\0');
        __atomic_store_n(&s_enabled, on, __ATOMIC_RELAXED);
    }
    if (!on) return;

    static __thread uint64_t t_head, t_t0, t_last;
    static __thread uint32_t t_hits;
    if (head != t_head || t_hits < EXTRA_SPIN_FAST_PASSES) {
        if (head != t_head) {
            t_head = head;
            t_hits = 1;
            t_t0 = 0;
            return;
        }
        t_hits++;
        return;
    }

    uint64_t now = extra_monotonic_ns();
    if (!t_t0 || now - t_last > EXTRA_SPIN_GAP_NS) {
        t_t0 = now;
        t_last = now;
        return;
    }
    t_last = now;
    uint64_t waited = now - t_t0;
    if (waited >= EXTRA_SPIN_SLEEP_NS) {
        uint64_t sleep_ns = waited >> 3;
        if (sleep_ns > EXTRA_SPIN_SLEEP_MAX_NS) sleep_ns = EXTRA_SPIN_SLEEP_MAX_NS;
        struct timespec req = {.tv_sec = 0, .tv_nsec = (long)sleep_ns};
        while (clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &req) == EINTR) {}
        t_last = extra_monotonic_ns();
        return;
    }
    if (waited >= EXTRA_SPIN_HANDOFF_NS) {
        sched_yield();
        t_last = extra_monotonic_ns();
    }
}
