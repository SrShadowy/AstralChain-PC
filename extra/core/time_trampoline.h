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
