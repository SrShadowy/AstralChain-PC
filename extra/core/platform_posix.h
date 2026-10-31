#pragma once
#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <wchar.h>
#include <dlfcn.h>
#include <signal.h>
#include <stdarg.h>

static inline int extra_is_channel_enabled(const char *s) {
    if (!s) return 0;
    static const char *const t[] = {"STOP","FATAL","fps","Error","error","CRASH","PL ","[vk]","[gpu]","[vulkan]","[present]","[post]","[video]","[audio]","[shaders]","[shader]","[backend]","[logic]","[app]","[input]","[prompts]","HID script"};
    for (size_t i = 0; i < sizeof(t)/sizeof(t[0]); i++) if (strstr(s, t[i])) return 1;
    return 0;
}
static inline int extra_printf(const char *fmt, ...) {
    static int v = -1; if (v < 0) { const char *e = getenv("AC_VERBOSE"); v = (e && (e[0] == '1' || e[0] == 'y')) ? 1 : 0; }
    if (!v && !extra_is_channel_enabled(fmt)) return 0;
    va_list a; va_start(a, fmt); int r = vprintf(fmt, a); va_end(a); return r;
}
static inline int extra_puts(const char *s) {
    static int v = -1; if (v < 0) { const char *e = getenv("AC_VERBOSE"); v = (e && (e[0] == '1' || e[0] == 'y')) ? 1 : 0; }
    if (!v && !extra_is_channel_enabled(s)) return 0;
    return puts(s);
}
#pragma GCC diagnostic ignored "-Wignored-attributes"
#define printf extra_printf
#define puts extra_puts

#include "sync_trampoline.h"
#include "memory_trampoline.h"

#define WINAPI
#define CALLBACK
typedef uint32_t DWORD; typedef int BOOL; typedef unsigned int UINT; typedef uint16_t WORD;
typedef int32_t LONG; typedef unsigned long ULONG; typedef uintptr_t ULONG_PTR, DWORD_PTR;
typedef unsigned char *PUCHAR; typedef void *HMODULE, *PVOID, *LPVOID; typedef int (*FARPROC)(void);
typedef uint64_t ULONGLONG, ULONG64; typedef int64_t LONG64, LONGLONG; typedef size_t SIZE_T;
#include "crypto_trampoline.h"

typedef union _LARGE_INTEGER { struct { DWORD LowPart; LONG HighPart; }; int64_t QuadPart; } LARGE_INTEGER;
typedef union _ULARGE_INTEGER { struct { DWORD LowPart; DWORD HighPart; }; uint64_t QuadPart; } ULARGE_INTEGER;

#define TRUE 1
#define FALSE 0
#define MAX_PATH 4096
#define ERROR_NOT_ENOUGH_MEMORY 8
static __thread DWORD g_last_win32_error = 0;
static inline DWORD GetLastError(void) { return g_last_win32_error; }
static inline void SetLastError(DWORD err) { g_last_win32_error = err; }
#include "crash_trampoline.h"

typedef struct _FILETIME { DWORD dwLowDateTime, dwHighDateTime; } FILETIME, *PFILETIME, *LPFILETIME;
static inline void GetSystemTimePreciseAsFileTime(LPFILETIME lpSystemTimeAsFileTime) {
    if (!lpSystemTimeAsFileTime) return;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t intervals = ((uint64_t)ts.tv_sec + 11644473600ULL) * 10000000ULL + (uint64_t)ts.tv_nsec / 100ULL;
    lpSystemTimeAsFileTime->dwLowDateTime = (DWORD)intervals;
    lpSystemTimeAsFileTime->dwHighDateTime = (DWORD)(intervals >> 32);
}

#define CP_UTF8 65001
#define MB_ERR_INVALID_CHARS 0x00000008
static inline int MultiByteToWideChar(UINT cp, DWORD flags, const char *src, int srclen, wchar_t *dst, int dstlen) {
    (void)cp; (void)flags; if (!src) return 0;
    if (srclen < 0) srclen = (int)strlen(src) + 1;
    if (dstlen == 0) return srclen;
    int n = srclen < dstlen ? srclen : dstlen;
    for (int i = 0; i < n; i++) dst[i] = (wchar_t)(unsigned char)src[i];
    return n;
}
static inline int WideCharToMultiByte(UINT cp, DWORD flags, const wchar_t *src, int srclen, char *dst, int dstlen, const char *defc, BOOL *used) {
    (void)cp; (void)flags; (void)defc; (void)used; if (!src) return 0;
    if (srclen < 0) { srclen = 0; while (src[srclen]) srclen++; srclen++; }
    if (dstlen == 0) return srclen;
    int n = srclen < dstlen ? srclen : dstlen;
    for (int i = 0; i < n; i++) dst[i] = (char)src[i];
    return n;
}

typedef struct _MEMORY_BASIC_INFORMATION {
    PVOID BaseAddress, AllocationBase; DWORD AllocationProtect; SIZE_T RegionSize; DWORD State, Protect, Type;
} MEMORY_BASIC_INFORMATION, *PMEMORY_BASIC_INFORMATION;

#define MEM_COMMIT 0x00001000
#define MEM_RESERVE 0x00002000
#define MEM_DECOMMIT 0x00004000
#define MEM_RELEASE 0x00008000
#define MEM_WRITE_WATCH 0x00200000
#define MEM_RESERVE_PLACEHOLDER 0x00040000
#define MEM_REPLACE_PLACEHOLDER 0x00000001
#define MEM_PRESERVE_PLACEHOLDER 0x00000002
typedef struct _MEM_EXTENDED_PARAMETER { ULONG_PTR Type; ULONG_PTR Reserved; } MEM_EXTENDED_PARAMETER;

#define PAGE_NOACCESS 0x01
#define PAGE_READONLY 0x02
#define PAGE_READWRITE 0x04
#define PAGE_EXECUTE_READ 0x20
#define PAGE_EXECUTE_READWRITE 0x40

static inline SIZE_T VirtualQuery(const void *addr, PMEMORY_BASIC_INFORMATION info, SIZE_T len) {
    (void)addr; (void)len;
    if (info) {
        memset(info, 0, sizeof(*info));
        info->State = MEM_COMMIT;
        info->Protect = PAGE_READWRITE;
        info->BaseAddress = (void*)(uintptr_t)addr;
        info->RegionSize = 16 * 1024 * 1024;
    }
    return sizeof(*info);
}

static inline void *VirtualAlloc(void *lpAddress, size_t dwSize, uint32_t flAllocationType, uint32_t flProtect) {
    (void)flProtect;
    if (!lpAddress) return extra_alloc_anonymous(dwSize);
    if (flAllocationType & MEM_RESERVE) return extra_reserve_guest_memory((uint64_t)(uintptr_t)lpAddress, dwSize);
    if (flAllocationType & MEM_COMMIT) return extra_commit_memory((uint64_t)(uintptr_t)lpAddress, dwSize) ? lpAddress : NULL;
    return NULL;
}

static inline int VirtualFree(void *lpAddress, size_t dwSize, uint32_t dwFreeType) {
    if (dwFreeType & MEM_PRESERVE_PLACEHOLDER) return 1;
    if (dwFreeType & MEM_DECOMMIT) return extra_decommit_memory((uint64_t)(uintptr_t)lpAddress, dwSize);
    if (dwFreeType & MEM_RELEASE) return extra_free_memory((uint64_t)(uintptr_t)lpAddress, dwSize);
    return 1;
}

static inline int VirtualProtect(void *lpAddress, size_t dwSize, uint32_t flNewProtect, uint32_t *lpflOldProtect) {
    (void)lpflOldProtect;
    int noaccess = (flNewProtect == PAGE_NOACCESS);
    return extra_protect_pages((uint64_t)(uintptr_t)lpAddress, dwSize, noaccess);
}

#define GENERIC_WRITE 0x40000000
#define CREATE_ALWAYS 2
#define FILE_ATTRIBUTE_NORMAL 0x80
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define DUPLICATE_SAME_ACCESS 0x00000002

#define LoadLibraryA(name) ((HMODULE)NULL)
#define LoadLibraryW(name) ((HMODULE)(uintptr_t)1)
#define LoadLibraryExW(name, h, f) ((HMODULE)NULL)
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#define FreeLibrary(m) (1)
#define GetModuleHandleA(n) ((HMODULE)(uintptr_t)1)
#define GetModuleHandleW(n) ((HMODULE)(uintptr_t)1)
#define GetModuleHandleExA(flags, addr, mod) (0)
#define GetModuleFileNameA(mod, buf, size) (0)
#define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 0
#define GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT 0
#define CreateFileA(name, access, share, sec, disp, flags, tmpl) ((HANDLE)0)

#include "shared_memory_trampoline.h"

static inline BOOL DuplicateHandle(HANDLE sp, HANDLE s, HANDLE dp, HANDLE *d, DWORD a, BOOL i, DWORD o) {
    (void)sp; (void)dp; (void)a; (void)i; (void)o;
    if (d) { *d = s; if (s && s != INVALID_HANDLE_VALUE && s != (HANDLE)(intptr_t)-1 && s != (HANDLE)(uintptr_t)1 && s != (HANDLE)(uintptr_t)2) s->ref_count++; }
    return 1;
}

static inline int extra_posix_stretch_dib(void *a, int b, int c, int d, int e, int f, int g, int h, int i, const void *j, const void *k, unsigned int l, uint32_t m) { (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h; (void)i; (void)j; (void)k; (void)l; (void)m; return 1; }

static inline FARPROC GetProcAddress(HMODULE mod, const char *name) {
    (void)mod;
    if (!name) return NULL;
    if (!strcmp(name, "VirtualAlloc2")) return (FARPROC)extra_posix_virtual_alloc2;
    if (!strcmp(name, "MapViewOfFile3")) return (FARPROC)extra_posix_map_view_of_file3;
    if (!strcmp(name, "UnmapViewOfFile2")) return (FARPROC)extra_posix_unmap_view_of_file2;
    if (!strcmp(name, "StretchDIBits")) return (FARPROC)extra_posix_stretch_dib;
    return (FARPROC)dlsym(RTLD_DEFAULT, name);
}

#define GetCurrentThread() ((HANDLE)(uintptr_t)2)
#define SetThreadPriority(t, p) (1)
#define THREAD_PRIORITY_BELOW_NORMAL (-1)
#define InterlockedExchange(t, v) __atomic_exchange_n((int32_t*)(t), (int32_t)(v), __ATOMIC_SEQ_CST)
#define InterlockedCompareExchange(dest, exch, comp) __sync_val_compare_and_swap((int32_t*)(dest), (int32_t)(comp), (int32_t)(exch))

#define TerminateProcess(p, e) exit(e)
#define GetCurrentProcess() ((HANDLE)(uintptr_t)1)
#define ExitProcess(e) exit(e)
#define CreateThread(sa, ss, fn, param, cf, tid) ((HANDLE)0)
#define SuspendThread(t) ((DWORD)0)
#define ResumeThread(t) ((DWORD)0)
#define GetThreadContext(t, c) (0)
#ifndef ALL_PROCESSOR_GROUPS
#define ALL_PROCESSOR_GROUPS 0xffff
#endif
static inline DWORD GetActiveProcessorCount(WORD group) {
    (void)group;
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (DWORD)n : 1;
}

typedef struct _SYSTEM_INFO {
    DWORD dwPageSize;
    LPVOID lpMinimumApplicationAddress, lpMaximumApplicationAddress;
    DWORD_PTR dwActiveProcessorMask;
    DWORD dwNumberOfProcessors, dwProcessorType, dwAllocationGranularity;
    WORD wProcessorLevel, wProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;

static inline void GetSystemInfo(LPSYSTEM_INFO info) {
    if (!info) return;
    memset(info, 0, sizeof(*info));
    info->dwPageSize = 4096;
    info->dwAllocationGranularity = 65536;
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    info->dwNumberOfProcessors = n > 0 ? (DWORD)n : 1;
}

#define _stricmp(a, b) strcasecmp(a, b)
#define _strdup(s) strdup(s)
static inline FILE *_wfopen(const wchar_t *path, const wchar_t *mode) {
    char p[4096], m[32];
    for (int i = 0; path[i] && i < 4095; i++) p[i] = (path[i] == L'\\') ? '/' : (char)path[i];
    p[wcslen(path) < 4095 ? wcslen(path) : 4095] = '\0';
    for (int i = 0; mode[i] && i < 31; i++) m[i] = (char)mode[i];
    m[wcslen(mode) < 31 ? wcslen(mode) : 31] = '\0';
    return fopen(p, m);
}



#define SetEvent(ev) extra_set_event(ev)
#define ResetEvent(ev) extra_reset_event(ev)
#define CloseHandle(h) extra_close_handle(h)
#define CreateEventW(sa, mr, is, nm) extra_create_event(mr, is)
#define WaitForSingleObject(h, ms) extra_wait_single(h, ms)
#define WaitForMultipleObjects(cnt, h, waitall, ms) extra_wait_multiple(cnt, h, waitall, ms)
static inline BOOL QueryPerformanceCounter(LARGE_INTEGER *p) { if (p) p->QuadPart = (int64_t)extra_qpc(); return TRUE; }
static inline BOOL QueryPerformanceFrequency(LARGE_INTEGER *p) { if (p) p->QuadPart = (int64_t)extra_qpf(); return TRUE; }
#define GetTickCount64() extra_tick_count()
#define GetCurrentThreadId() extra_current_thread_id()
#define GetCurrentProcessId() ((DWORD)getpid())
#define Sleep(ms) usleep((ms) * 1000)

#define AcquireSRWLockExclusive(l) pthread_mutex_lock(l)
#define ReleaseSRWLockExclusive(l) pthread_mutex_unlock(l)
#define TryAcquireSRWLockExclusive(l) (pthread_mutex_trylock(l) == 0)
#define SleepConditionVariableSRW(cv, l, t, f) extra_sleep_cv(cv, l, t)
#define WakeConditionVariable(cv) pthread_cond_signal(cv)

#define InterlockedExchange64(t, v) __atomic_exchange_n((int64_t*)(t), (int64_t)(v), __ATOMIC_SEQ_CST)
#define InterlockedIncrement(t) __atomic_add_fetch((int32_t*)(t), 1, __ATOMIC_SEQ_CST)
#define InterlockedDecrement(t) __atomic_sub_fetch((int32_t*)(t), 1, __ATOMIC_SEQ_CST)

static inline DWORD GetEnvironmentVariableA(const char *name, char *buf, DWORD size) {
    const char *val = getenv(name);
    if (!val) return 0;
    DWORD len = (DWORD)strlen(val);
    if (buf && size > len) memcpy(buf, val, len + 1);
    return len;
}

static inline DWORD GetEnvironmentVariableW(const wchar_t *name, wchar_t *buf, DWORD size) {
    (void)name; (void)buf; (void)size;
    return 0;
}

typedef struct { const char *name; void *fn; } ExtraNvnLinkedEntry;
__attribute__((used, retain, section("hle_entries"))) static const ExtraNvnLinkedEntry __extra_hle_anchor = {0, 0};
extern const ExtraNvnLinkedEntry __start_hle_entries[] __attribute__((weak));
extern const ExtraNvnLinkedEntry __stop_hle_entries[] __attribute__((weak));
static inline void *extra_nvn_linked_lookup(const char *name) {
    if (!__start_hle_entries || !__stop_hle_entries) return NULL;
    for (const ExtraNvnLinkedEntry *e = __start_hle_entries; e < __stop_hle_entries; e++) {
        if (e->name && e->fn && !strcmp(e->name, name)) return e->fn;
    }
    return NULL;
}

static void __extra_term_handler(int sig) { (void)sig; _exit(0); }
__attribute__((constructor)) static void __extra_init_posix_signals(void) {
    setvbuf(stdout, NULL, _IOLBF, 0); signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, __extra_term_handler); signal(SIGINT, __extra_term_handler);
}
