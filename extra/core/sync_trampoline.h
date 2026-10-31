#pragma once
#define _GNU_SOURCE
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <dirent.h>
#include <sched.h>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

#define INFINITE 0xFFFFFFFFu
#define WAIT_OBJECT_0 0u
#define WAIT_TIMEOUT 0x00000102u
#define WAIT_FAILED 0xFFFFFFFFu

typedef enum {
    EXTRA_HANDLE_EVENT = 1,
    EXTRA_HANDLE_FILE = 2,
    EXTRA_HANDLE_FIND = 3,
    EXTRA_HANDLE_MAPPING = 4
} ExtraHandleType;

typedef struct ExtraHandle {
    ExtraHandleType type;
    int ref_count;
    union {
        struct {
            pthread_mutex_t lock;
            pthread_cond_t cond;
            int signaled;
            int manual_reset;
        } event;
        struct {
            int fd;
        } file;
        struct {
            void *dir;
            char base_path[1024];
            char pattern[64];
        } find;
        struct {
            int fd;
            size_t size;
        } mapping;
    };
} ExtraHandle;

typedef ExtraHandle *HANDLE;
typedef pthread_mutex_t SRWLOCK;
#define SRWLOCK_INIT PTHREAD_MUTEX_INITIALIZER
typedef pthread_cond_t CONDITION_VARIABLE;

static inline HANDLE extra_create_event(int manual_reset, int initial_state) {
    ExtraHandle *ev = (ExtraHandle *)calloc(1, sizeof(ExtraHandle));
    if (!ev) return NULL;
    ev->type = EXTRA_HANDLE_EVENT;
    ev->ref_count = 1;
    pthread_mutex_init(&ev->event.lock, NULL);
    pthread_cond_init(&ev->event.cond, NULL);
    ev->event.manual_reset = manual_reset;
    ev->event.signaled = initial_state ? 1 : 0;
    return ev;
}

static inline int extra_set_event(HANDLE ev) {
    if (!ev || ev->type != EXTRA_HANDLE_EVENT) return 0;
    pthread_mutex_lock(&ev->event.lock);
    ev->event.signaled = 1;
    pthread_cond_broadcast(&ev->event.cond);
    pthread_mutex_unlock(&ev->event.lock);
    return 1;
}

static inline int extra_reset_event(HANDLE ev) {
    if (!ev || ev->type != EXTRA_HANDLE_EVENT) return 0;
    pthread_mutex_lock(&ev->event.lock);
    ev->event.signaled = 0;
    pthread_mutex_unlock(&ev->event.lock);
    return 1;
}

static inline int extra_close_handle(HANDLE h) {
    if (!h) return 0;
    if (h == (HANDLE)(intptr_t)-1 || h == (HANDLE)(uintptr_t)1 || h == (HANDLE)(uintptr_t)2) return 1;
    if (h->ref_count > 1) {
        h->ref_count--;
        return 1;
    }
    if (h->type == EXTRA_HANDLE_EVENT) {
        pthread_mutex_lock(&h->event.lock);
        pthread_cond_destroy(&h->event.cond);
        pthread_mutex_unlock(&h->event.lock);
        pthread_mutex_destroy(&h->event.lock);
    } else if (h->type == EXTRA_HANDLE_FILE) {
        if (h->file.fd >= 0) close(h->file.fd);
    } else if (h->type == EXTRA_HANDLE_FIND) {
        if (h->find.dir) closedir((DIR *)h->find.dir);
    } else if (h->type == EXTRA_HANDLE_MAPPING) {
        if (h->mapping.fd >= 0) close(h->mapping.fd);
    }
    free(h);
    return 1;
}

static inline uint32_t extra_wait_single(HANDLE ev, uint32_t timeout_ms) {
    if (!ev || ev->type != EXTRA_HANDLE_EVENT) return WAIT_FAILED;
    pthread_mutex_lock(&ev->event.lock);
    if (ev->event.signaled) {
        if (!ev->event.manual_reset) ev->event.signaled = 0;
        pthread_mutex_unlock(&ev->event.lock);
        return WAIT_OBJECT_0;
    }
    if (timeout_ms == 0) {
        pthread_mutex_unlock(&ev->event.lock);
        return WAIT_TIMEOUT;
    }

    struct timespec ts;
    if (timeout_ms != INFINITE) {
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000L;
        }
    }

    while (!ev->event.signaled) {
        if (timeout_ms == INFINITE) {
            pthread_cond_wait(&ev->event.cond, &ev->event.lock);
        } else {
            int ret = pthread_cond_timedwait(&ev->event.cond, &ev->event.lock, &ts);
            if (ret == ETIMEDOUT) {
                pthread_mutex_unlock(&ev->event.lock);
                return WAIT_TIMEOUT;
            }
        }
    }

    if (!ev->event.manual_reset) ev->event.signaled = 0;
    pthread_mutex_unlock(&ev->event.lock);
    return WAIT_OBJECT_0;
}

static inline uint32_t extra_wait_multiple(uint32_t count, const HANDLE *handles, int wait_all, uint32_t timeout_ms) {
    (void)wait_all;
    if (!count || !handles) return WAIT_FAILED;
    uint64_t start = 0;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    start = (uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_nsec / 1000000u;

    for (;;) {
        for (uint32_t i = 0; i < count; i++) {
            if (handles[i] && extra_wait_single(handles[i], 0) == WAIT_OBJECT_0) {
                return WAIT_OBJECT_0 + i;
            }
        }
        if (timeout_ms == 0) return WAIT_TIMEOUT;
        clock_gettime(CLOCK_MONOTONIC, &now);
        uint64_t elapsed = ((uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_nsec / 1000000u) - start;
        if (timeout_ms != INFINITE && elapsed >= timeout_ms) return WAIT_TIMEOUT;
        usleep(500);
    }
}

static inline int extra_sleep_cv(CONDITION_VARIABLE *cv, SRWLOCK *lock, uint32_t timeout_ms) {
    if (timeout_ms == INFINITE) {
        return pthread_cond_wait(cv, lock) == 0;
    }
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }
    return pthread_cond_timedwait(cv, lock, &ts) == 0;
}

static inline uint64_t extra_qpc(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static inline uint64_t extra_qpf(void) {
    return 1000000000ull;
}

static inline uint64_t extra_tick_count(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)ts.tv_nsec / 1000000ull;
}

static inline uint32_t extra_current_thread_id(void) {
    return (uint32_t)(uintptr_t)pthread_self();
}

static inline uint64_t extra_allocate_tls(uint64_t tls, uint64_t alignment) {
    void *allocation = mmap((void *)(uintptr_t)tls, alignment, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (allocation == (void *)(uintptr_t)tls) return tls;
    if (allocation != MAP_FAILED) munmap(allocation, alignment);
    return 0;
}

static inline void extra_parallel_start(unsigned i, void *(*main_fn)(void *)) {
    pthread_t th;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 16u << 20);
    pthread_create(&th, &attr, main_fn, (void *)(uintptr_t)i);
    pthread_attr_destroy(&attr);
    printf("PARALLEL guest thread %u on host thread %lu\n", i, (unsigned long)th);
    pthread_detach(th);
}

static inline int extra_switch_to_thread(void) { sched_yield(); return 1; }
#define SwitchToThread() extra_switch_to_thread()
#define ExitThread(code) do { (void)(code); pthread_exit(NULL); } while(0)

