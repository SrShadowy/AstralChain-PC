#pragma once
#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#define EXTRA_BG_THREAD_NICE 10
#define EXTRA_DUMMY_PROCESS_HANDLE ((HANDLE)(uintptr_t)1)
#define EXTRA_DUMMY_THREAD_HANDLE ((HANDLE)(uintptr_t)2)

static inline int extra_is_channel_enabled(const char *s) {
    if (!s) {
        return 0;
    }
    static const char *const t[] = {
        "STOP", "FATAL", "fps", "Error", "error", "CRASH", "PL ",
        "[vk]", "[gpu]", "[vulkan]", "[present]", "[post]", "[video]",
        "[audio]", "[shaders]", "[shader]", "[backend]", "[logic]",
        "[app]", "[input]", "[prompts]", "HID script",
        "progress ", "done ", "warm-up:"
    };
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        if (strstr(s, t[i])) {
            return 1;
        }
    }
    return 0;
}

static inline int extra_is_verbose_log_enabled(void) {
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("AC_VERBOSE");
        v = (e && (e[0] == '1' || e[0] == 'y')) ? 1 : 0;
    }
    return v;
}

static inline int extra_printf(const char *fmt, ...) {
    if (!extra_is_verbose_log_enabled() && !extra_is_channel_enabled(fmt)) {
        return 0;
    }
    va_list a;
    va_start(a, fmt);
    int r = vprintf(fmt, a);
    va_end(a);
    return r;
}

static inline int extra_puts(const char *s) {
    if (!extra_is_verbose_log_enabled() && !extra_is_channel_enabled(s)) {
        return 0;
    }
    return puts(s);
}

#pragma GCC diagnostic ignored "-Wignored-attributes"
#define printf extra_printf
#define puts extra_puts

#include "sync_trampoline.h"
#include "memory_trampoline.h"
#include "win32_types.h"
#include "crypto_trampoline.h"

static __thread DWORD g_last_win32_error = 0;

static inline DWORD GetLastError(void) {
    return g_last_win32_error;
}

static inline void SetLastError(DWORD err) {
    g_last_win32_error = err;
}

#include "crash_trampoline.h"

#define MEM_COMMIT 0x00001000
#define MEM_RESERVE 0x00002000
#define MEM_DECOMMIT 0x00004000
#define MEM_RELEASE 0x00008000
#define MEM_WRITE_WATCH 0x00200000
#define MEM_RESERVE_PLACEHOLDER 0x00040000
#define MEM_REPLACE_PLACEHOLDER 0x00000001
#define MEM_PRESERVE_PLACEHOLDER 0x00000002

#define PAGE_NOACCESS 0x01
#define PAGE_READONLY 0x02
#define PAGE_READWRITE 0x04
#define PAGE_EXECUTE_READ 0x20
#define PAGE_EXECUTE_READWRITE 0x40

static inline SIZE_T VirtualQuery(const void *addr, PMEMORY_BASIC_INFORMATION info, SIZE_T len) {
    (void)len;
    if (info) {
        memset(info, 0, sizeof(*info));
        info->State = MEM_COMMIT;
        info->Protect = PAGE_READWRITE;
        info->BaseAddress = (void *)(uintptr_t)addr;
        info->RegionSize = EXTRA_WIN32_DEFAULT_REGION_SIZE;
    }
    return sizeof(*info);
}

static inline void *VirtualAlloc(void *lpAddress, size_t dwSize, uint32_t flAllocationType, uint32_t flProtect) {
    (void)flProtect;
    if (!lpAddress) {
        return extra_alloc_anonymous(dwSize);
    }
    if (flAllocationType & MEM_RESERVE) {
        return extra_reserve_guest_memory((uint64_t)(uintptr_t)lpAddress, dwSize);
    }
    if (flAllocationType & MEM_COMMIT) {
        return extra_commit_memory((uint64_t)(uintptr_t)lpAddress, dwSize) ? lpAddress : NULL;
    }
    return NULL;
}

static inline int VirtualFree(void *lpAddress, size_t dwSize, uint32_t dwFreeType) {
    if (dwFreeType & MEM_PRESERVE_PLACEHOLDER) {
        return 1;
    }
    if (dwFreeType & MEM_DECOMMIT) {
        return extra_decommit_memory((uint64_t)(uintptr_t)lpAddress, dwSize);
    }
    if (dwFreeType & MEM_RELEASE) {
        return extra_free_memory((uint64_t)(uintptr_t)lpAddress, dwSize);
    }
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
    (void)sp;
    (void)dp;
    (void)a;
    (void)i;
    (void)o;
    if (d) {
        *d = s;
        if (s && s != INVALID_HANDLE_VALUE && s != EXTRA_DUMMY_PROCESS_HANDLE && s != EXTRA_DUMMY_THREAD_HANDLE) {
            s->ref_count++;
        }
    }
    return 1;
}

static inline int extra_posix_stretch_dib(
    void *a, int b, int c, int d, int e, int f, int g, int h, int i,
    const void *j, const void *k, unsigned int l, uint32_t m
) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g;
    (void)h; (void)i; (void)j; (void)k; (void)l; (void)m;
    return 1;
}

static inline FARPROC GetProcAddress(HMODULE mod, const char *name) {
    (void)mod;
    if (!name) {
        return NULL;
    }
    if (!strcmp(name, "VirtualAlloc2")) {
        return (FARPROC)extra_posix_virtual_alloc2;
    }
    if (!strcmp(name, "MapViewOfFile3")) {
        return (FARPROC)extra_posix_map_view_of_file3;
    }
    if (!strcmp(name, "UnmapViewOfFile2")) {
        return (FARPROC)extra_posix_unmap_view_of_file2;
    }
    if (!strcmp(name, "StretchDIBits")) {
        return (FARPROC)extra_posix_stretch_dib;
    }
    return (FARPROC)dlsym(RTLD_DEFAULT, name);
}

static inline BOOL extra_set_thread_priority(HANDLE t, int prio) {
    (void)t;
    if (prio < 0) {
        setpriority(PRIO_PROCESS, 0, EXTRA_BG_THREAD_NICE);
    }
    return TRUE;
}

#define GetCurrentThread() EXTRA_DUMMY_THREAD_HANDLE
#define SetThreadPriority(t, p) extra_set_thread_priority((t), (p))
#define THREAD_PRIORITY_BELOW_NORMAL (-1)
#define THREAD_PRIORITY_NORMAL 0
#define THREAD_PRIORITY_ABOVE_NORMAL 1
#define THREAD_PRIORITY_HIGHEST 2
#define InterlockedExchange(t, v) __atomic_exchange_n((int32_t *)(t), (int32_t)(v), __ATOMIC_SEQ_CST)
#define InterlockedCompareExchange(dest, exch, comp) __sync_val_compare_and_swap((int32_t *)(dest), (int32_t)(comp), (int32_t)(exch))

#define TerminateProcess(p, e) exit(e)
#define GetCurrentProcess() EXTRA_DUMMY_PROCESS_HANDLE
#define ExitProcess(e) exit(e)
#define CreateThread(sa, ss, fn, param, cf, tid) ((HANDLE)0)
#define SuspendThread(t) ((DWORD)0)
#define ResumeThread(t) ((DWORD)0)
#define GetThreadContext(t, c) (0)

#define _stricmp(a, b) strcasecmp(a, b)
#define _strdup(s) strdup(s)

#define SetEvent(ev) extra_set_event(ev)
#define ResetEvent(ev) extra_reset_event(ev)
#define CloseHandle(h) extra_close_handle(h)
#define CreateEventW(sa, mr, is, nm) extra_create_event(mr, is)
#define WaitForSingleObject(h, ms) extra_wait_single(h, ms)
#define WaitForMultipleObjects(cnt, h, waitall, ms) extra_wait_multiple(cnt, h, waitall, ms)

static inline BOOL QueryPerformanceCounter(LARGE_INTEGER *p) {
    if (p) {
        p->QuadPart = (int64_t)extra_qpc();
    }
    return TRUE;
}

static inline BOOL QueryPerformanceFrequency(LARGE_INTEGER *p) {
    if (p) {
        p->QuadPart = (int64_t)extra_qpf();
    }
    return TRUE;
}

#define GetTickCount64() extra_tick_count()
#define GetCurrentThreadId() extra_current_thread_id()
#define GetCurrentProcessId() ((DWORD)getpid())

static inline void extra_sleep_ms(unsigned int ms) {
    if (ms == 0) {
        sched_yield();
    } else {
        struct timespec ts = {
            .tv_sec = (time_t)(ms / EXTRA_MS_PER_SEC),
            .tv_nsec = (long)(ms % EXTRA_MS_PER_SEC) * EXTRA_NS_PER_MS
        };
        while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {}
    }
}

#define Sleep(ms) extra_sleep_ms(ms)

#define AcquireSRWLockExclusive(l) pthread_mutex_lock(l)
#define ReleaseSRWLockExclusive(l) pthread_mutex_unlock(l)
#define TryAcquireSRWLockExclusive(l) (pthread_mutex_trylock(l) == 0)
#define SleepConditionVariableSRW(cv, l, t, f) extra_sleep_cv(cv, l, t)
#define WakeConditionVariable(cv) pthread_cond_signal(cv)

#define InterlockedExchange64(t, v) __atomic_exchange_n((int64_t *)(t), (int64_t)(v), __ATOMIC_SEQ_CST)
#define InterlockedIncrement(t) __atomic_add_fetch((int32_t *)(t), 1, __ATOMIC_SEQ_CST)
#define InterlockedDecrement(t) __atomic_sub_fetch((int32_t *)(t), 1, __ATOMIC_SEQ_CST)

typedef struct {
    const char *name;
    void *fn;
} ExtraNvnLinkedEntry;

__attribute__((used, retain, section("hle_entries"))) static const ExtraNvnLinkedEntry __extra_hle_anchor = {0, 0};
extern const ExtraNvnLinkedEntry __start_hle_entries[] __attribute__((weak));
extern const ExtraNvnLinkedEntry __stop_hle_entries[] __attribute__((weak));

static inline void *extra_nvn_linked_lookup(const char *name) {
    if (!__start_hle_entries || !__stop_hle_entries) {
        return NULL;
    }
    for (const ExtraNvnLinkedEntry *e = __start_hle_entries; e < __stop_hle_entries; e++) {
        if (e->name && e->fn && !strcmp(e->name, name)) {
            return e->fn;
        }
    }
    return NULL;
}

static void __extra_term_handler(int sig) {
    (void)sig;
    _exit(0);
}

__attribute__((constructor)) static void __extra_init_posix_signals(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, __extra_term_handler);
    signal(SIGINT, __extra_term_handler);
}
