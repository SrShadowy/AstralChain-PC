#pragma once
#define _GNU_SOURCE
#include <sys/mman.h>
#include <signal.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

static int (*g_lazy_fault_hook)(uint64_t addr) = NULL;

static void extra_sigsegv_handler(int sig, siginfo_t *info, void *ucontext) {
    (void)ucontext;
    if (sig == SIGSEGV && g_lazy_fault_hook) {
        if (g_lazy_fault_hook((uint64_t)info->si_addr)) {
            return;
        }
    }
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGSEGV, &sa, NULL);
    raise(SIGSEGV);
}

static inline void *extra_reserve_guest_memory(uint64_t addr, size_t size) {
    void *ptr = mmap((void *)(uintptr_t)addr, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
    if (ptr == MAP_FAILED) return NULL;
    if (ptr != (void *)(uintptr_t)addr) {
        munmap(ptr, size);
        return NULL;
    }
    madvise(ptr, size, MADV_NOHUGEPAGE);
    return ptr;
}

static inline int extra_commit_memory(uint64_t addr, size_t size) {
    return mprotect((void *)(uintptr_t)addr, size, PROT_READ | PROT_WRITE) == 0;
}

static inline int extra_decommit_memory(uint64_t addr, size_t size) {
    return madvise((void *)(uintptr_t)addr, size, MADV_DONTNEED) == 0;
}

#include <pthread.h>

#define EXTRA_MAX_TRACKED_ALLOCS 256
typedef struct {
    void *ptr;
    size_t size;
} ExtraAllocTrack;

static ExtraAllocTrack g_extra_allocs[EXTRA_MAX_TRACKED_ALLOCS];
static pthread_mutex_t g_extra_alloc_lock = PTHREAD_MUTEX_INITIALIZER;

static inline void *extra_alloc_anonymous(size_t size) {
    void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) return NULL;
    pthread_mutex_lock(&g_extra_alloc_lock);
    for (int i = 0; i < EXTRA_MAX_TRACKED_ALLOCS; i++) {
        if (!g_extra_allocs[i].ptr) {
            g_extra_allocs[i].ptr = ptr;
            g_extra_allocs[i].size = size;
            break;
        }
    }
    pthread_mutex_unlock(&g_extra_alloc_lock);
    return ptr;
}

static inline int extra_free_anonymous(void *ptr, size_t size) {
    if (!ptr) return 1;
    size_t sz = size;
    pthread_mutex_lock(&g_extra_alloc_lock);
    for (int i = 0; i < EXTRA_MAX_TRACKED_ALLOCS; i++) {
        if (g_extra_allocs[i].ptr == ptr) {
            if (!sz) sz = g_extra_allocs[i].size;
            g_extra_allocs[i].ptr = NULL;
            g_extra_allocs[i].size = 0;
            break;
        }
    }
    pthread_mutex_unlock(&g_extra_alloc_lock);
    if (!sz) sz = 65536;
    return munmap(ptr, sz) == 0;
}

static inline int extra_free_memory(uint64_t addr, size_t size) {
    return extra_free_anonymous((void *)(uintptr_t)addr, size);
}

static inline int extra_protect_pages(uint64_t addr, size_t size, int noaccess) {
    static int handler_installed = 0;
    if (!handler_installed) {
        handler_installed = 1;
        struct sigaction sa;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO;
        sa.sa_sigaction = extra_sigsegv_handler;
        sigaction(SIGSEGV, &sa, NULL);
    }
    int prot = noaccess ? PROT_NONE : (PROT_READ | PROT_WRITE);
    return mprotect((void *)(uintptr_t)addr, size, prot) == 0;
}

static inline void extra_set_lazy_fault_handler(int (*hook)(uint64_t addr)) {
    g_lazy_fault_hook = hook;
}

