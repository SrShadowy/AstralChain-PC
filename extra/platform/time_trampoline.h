#pragma once
#include <stdint.h>
#include <time.h>
#include <string.h>

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
    wchar_t StandardName[32];
    SYSTEMTIME StandardDate;
    int32_t StandardBias;
    wchar_t DaylightName[32];
    SYSTEMTIME DaylightDate;
    int32_t DaylightBias;
} TIME_ZONE_INFORMATION;

static inline int _localtime64_s(struct tm *result, const __time64_t *timer) {
    if (!result || !timer) return 22;
    time_t t = (time_t)*timer;
    return localtime_r(&t, result) ? 0 : 22;
}

static inline uint32_t GetTimeZoneInformation(TIME_ZONE_INFORMATION *tzi) {
    if (!tzi) return 0xFFFFFFFFu;
    memset(tzi, 0, sizeof(*tzi));
    time_t t = time(NULL);
    struct tm local;
    localtime_r(&t, &local);
    tzi->Bias = (int32_t)(-local.tm_gmtoff / 60);
    tzi->StandardBias = 0;
    tzi->DaylightBias = 0;
    return 1;
}

static inline void GetLocalTime(SYSTEMTIME *st) {
    if (!st) return;
    time_t t = time(NULL);
    struct tm local;
    localtime_r(&t, &local);
    st->wYear = (uint16_t)(local.tm_year + 1900);
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
#define EXTRA_SPIN_WINDOW_NS  250000ULL /* 250 us spin window before target deadline */

/* Hybrid precise sleep until monotonic nanosecond target t.
 * Blocks via clock_nanosleep when > 250 us remains, then yields for final precision. */
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
        sched_yield();
    }
}

/* Hybrid sleep for svc #0xB (SleepThread / yield).
 * Replaces the Windows-legacy polling loop that burned cores in sched_yield. */
static inline void extra_svc_sleep_thread(int64_t ns) {
    if (ns <= 0) {
        sched_yield();
        return;
    }
    struct timespec target_ts;
    clock_gettime(CLOCK_MONOTONIC, &target_ts);
    target_ts.tv_sec += (time_t)(ns / EXTRA_TIME_NS_PER_SEC);
    target_ts.tv_nsec += (long)(ns % EXTRA_TIME_NS_PER_SEC);
    if (target_ts.tv_nsec >= (long)EXTRA_TIME_NS_PER_SEC) {
        target_ts.tv_sec += 1;
        target_ts.tv_nsec -= (long)EXTRA_TIME_NS_PER_SEC;
    }

    if ((uint64_t)ns > EXTRA_SPIN_WINDOW_NS) {
        struct timespec sleep_ts = target_ts;
        if (sleep_ts.tv_nsec >= (long)EXTRA_SPIN_WINDOW_NS) {
            sleep_ts.tv_nsec -= (long)EXTRA_SPIN_WINDOW_NS;
        } else {
            sleep_ts.tv_sec -= 1;
            sleep_ts.tv_nsec += ((long)EXTRA_TIME_NS_PER_SEC - (long)EXTRA_SPIN_WINDOW_NS);
        }
        while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &sleep_ts, NULL) == EINTR) {}
    }

    struct timespec now;
    for (;;) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > target_ts.tv_sec ||
            (now.tv_sec == target_ts.tv_sec && now.tv_nsec >= target_ts.tv_nsec)) {
            break;
        }
        sched_yield();
    }
}
