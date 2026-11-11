#pragma once
#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#define WINAPI
#define CALLBACK

typedef uint32_t DWORD;
typedef int BOOL;
typedef unsigned int UINT;
typedef uint16_t WORD;
typedef int32_t LONG;
typedef unsigned long ULONG;
typedef uintptr_t ULONG_PTR;
typedef uintptr_t DWORD_PTR;
typedef unsigned char *PUCHAR;
typedef void *HMODULE;
typedef void *PVOID;
typedef void *LPVOID;
typedef int (*FARPROC)(void);
typedef uint64_t ULONGLONG;
typedef uint64_t ULONG64;
typedef int64_t LONG64;
typedef int64_t LONGLONG;
typedef size_t SIZE_T;

typedef union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG HighPart;
    };
    int64_t QuadPart;
} LARGE_INTEGER;

typedef union _ULARGE_INTEGER {
    struct {
        DWORD LowPart;
        DWORD HighPart;
    };
    uint64_t QuadPart;
} ULARGE_INTEGER;

#define TRUE 1
#define FALSE 0
#define MAX_PATH 4096
#define ERROR_NOT_ENOUGH_MEMORY 8

#define EXTRA_WIN32_EPOCH_OFFSET_SEC 11644473600ULL
#define EXTRA_WIN32_100NS_PER_SEC 10000000ULL
#define EXTRA_WIN32_NS_PER_100NS 100ULL
#define EXTRA_WIN32_HIGH_DWORD_SHIFT 32
#define EXTRA_WIN32_PAGE_SIZE 4096
#define EXTRA_WIN32_ALLOC_GRANULARITY 65536
#define EXTRA_WIN32_DEFAULT_REGION_SIZE (16 * 1024 * 1024)
#define EXTRA_WFOPEN_MODE_CAP 32

typedef struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

static inline void GetSystemTimePreciseAsFileTime(LPFILETIME lpSystemTimeAsFileTime) {
    if (!lpSystemTimeAsFileTime) {
        return;
    }
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t intervals = ((uint64_t)ts.tv_sec + EXTRA_WIN32_EPOCH_OFFSET_SEC) * EXTRA_WIN32_100NS_PER_SEC +
                         (uint64_t)ts.tv_nsec / EXTRA_WIN32_NS_PER_100NS;
    lpSystemTimeAsFileTime->dwLowDateTime = (DWORD)intervals;
    lpSystemTimeAsFileTime->dwHighDateTime = (DWORD)(intervals >> EXTRA_WIN32_HIGH_DWORD_SHIFT);
}

#define CP_UTF8 65001
#define MB_ERR_INVALID_CHARS 0x00000008

static inline int MultiByteToWideChar(UINT cp, DWORD flags, const char *src, int srclen, wchar_t *dst, int dstlen) {
    (void)cp;
    (void)flags;
    if (!src) {
        return 0;
    }
    if (srclen < 0) {
        srclen = (int)strlen(src) + 1;
    }
    if (dstlen == 0) {
        return srclen;
    }
    int n = srclen < dstlen ? srclen : dstlen;
    for (int i = 0; i < n; i++) {
        dst[i] = (wchar_t)(unsigned char)src[i];
    }
    return n;
}

static inline int WideCharToMultiByte(
    UINT cp,
    DWORD flags,
    const wchar_t *src,
    int srclen,
    char *dst,
    int dstlen,
    const char *defc,
    BOOL *used
) {
    (void)cp;
    (void)flags;
    (void)defc;
    (void)used;
    if (!src) {
        return 0;
    }
    if (srclen < 0) {
        srclen = 0;
        while (src[srclen]) {
            srclen++;
        }
        srclen++;
    }
    if (dstlen == 0) {
        return srclen;
    }
    int n = srclen < dstlen ? srclen : dstlen;
    for (int i = 0; i < n; i++) {
        dst[i] = (char)src[i];
    }
    return n;
}

typedef struct _MEMORY_BASIC_INFORMATION {
    PVOID BaseAddress;
    PVOID AllocationBase;
    DWORD AllocationProtect;
    SIZE_T RegionSize;
    DWORD State;
    DWORD Protect;
    DWORD Type;
} MEMORY_BASIC_INFORMATION, *PMEMORY_BASIC_INFORMATION;

typedef struct _MEM_EXTENDED_PARAMETER {
    ULONG_PTR Type;
    ULONG_PTR Reserved;
} MEM_EXTENDED_PARAMETER;

typedef struct _SYSTEM_INFO {
    DWORD dwPageSize;
    LPVOID lpMinimumApplicationAddress;
    LPVOID lpMaximumApplicationAddress;
    DWORD_PTR dwActiveProcessorMask;
    DWORD dwNumberOfProcessors;
    DWORD dwProcessorType;
    DWORD dwAllocationGranularity;
    WORD wProcessorLevel;
    WORD wProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;

#ifndef ALL_PROCESSOR_GROUPS
#define ALL_PROCESSOR_GROUPS 0xffff
#endif

static inline DWORD GetActiveProcessorCount(WORD group) {
    (void)group;
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (DWORD)n : 1;
}

static inline void GetSystemInfo(LPSYSTEM_INFO info) {
    if (!info) {
        return;
    }
    memset(info, 0, sizeof(*info));
    info->dwPageSize = EXTRA_WIN32_PAGE_SIZE;
    info->dwAllocationGranularity = EXTRA_WIN32_ALLOC_GRANULARITY;
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    info->dwNumberOfProcessors = n > 0 ? (DWORD)n : 1;
}

static inline FILE *_wfopen(const wchar_t *path, const wchar_t *mode) {
    char p[MAX_PATH];
    char m[EXTRA_WFOPEN_MODE_CAP];
    for (int i = 0; path[i] && i < MAX_PATH - 1; i++) {
        p[i] = (path[i] == L'\\') ? '/' : (char)path[i];
    }
    size_t plen = wcslen(path);
    p[plen < MAX_PATH - 1 ? plen : MAX_PATH - 1] = '\0';
    for (int i = 0; mode[i] && i < EXTRA_WFOPEN_MODE_CAP - 1; i++) {
        m[i] = (char)mode[i];
    }
    size_t mlen = wcslen(mode);
    m[mlen < EXTRA_WFOPEN_MODE_CAP - 1 ? mlen : EXTRA_WFOPEN_MODE_CAP - 1] = '\0';
    return fopen(p, m);
}

static inline DWORD GetEnvironmentVariableA(const char *name, char *buf, DWORD size) {
    const char *val = getenv(name);
    if (!val) {
        return 0;
    }
    DWORD len = (DWORD)strlen(val);
    if (buf && size > len) {
        memcpy(buf, val, len + 1);
    }
    return len;
}

static inline DWORD GetEnvironmentVariableW(const wchar_t *name, wchar_t *buf, DWORD size) {
    (void)name;
    (void)buf;
    (void)size;
    return 0;
}
