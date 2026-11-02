#pragma once
#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <linux/fs.h>
#include <linux/userfaultfd.h>

#ifndef UFFD_USER_MODE_ONLY
#define UFFD_USER_MODE_ONLY 1
#endif
#ifndef UFFD_FEATURE_WP_ASYNC
#define UFFD_FEATURE_WP_ASYNC (1 << 15)
#endif
#ifndef PAGEMAP_SCAN
#define PAGEMAP_SCAN _IOWR(0x66, 16, struct pm_scan_arg)
#endif
#ifndef PAGE_IS_WRITTEN
#define PAGE_IS_WRITTEN (1 << 1)
#endif
#ifndef PM_SCAN_WP_MATCHING
#define PM_SCAN_WP_MATCHING (1 << 0)
#endif

#define EXTRA_TRACK_PAGE_SIZE 4096ULL
#define EXTRA_TRACK_MAX_REGIONS 512
#define EXTRA_TRACK_HEAP_LIMIT 0x100000000ULL /* 4 GB */

enum ExtraTrackMode {
    EXTRA_TRACK_MODE_UNSET = 0,
    EXTRA_TRACK_MODE_UFFD_WP,
    EXTRA_TRACK_MODE_SOFT_DIRTY,
    EXTRA_TRACK_MODE_PREAD,
    EXTRA_TRACK_MODE_FAILED
};

static enum ExtraTrackMode g_extra_track_mode = EXTRA_TRACK_MODE_UNSET;
static int g_extra_pagemap_fd = -1;
static int g_extra_uffd_fd = -1;
static int g_extra_clear_refs_fd = -1;
static uint64_t g_extra_last_cr_ns = 0;

static inline uint64_t extra_track_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* Initializes native Linux page write tracking.
 * Primary method: userfaultfd async WP (UFFD_FEATURE_WP_ASYNC) + PAGEMAP_SCAN PM_SCAN_WP_MATCHING (Linux 6.7+).
 * Fallback: soft-dirty PAGEMAP_SCAN or pread with throttled clear_refs. */
static inline void extra_track_init(uint64_t heap_base) {
    if (g_extra_track_mode != EXTRA_TRACK_MODE_UNSET) return;

    /* 1. Try Linux 6.7+ userfaultfd async WP (per-range reset, microsecond fast) */
    int uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK | UFFD_USER_MODE_ONLY);
    if (uffd >= 0) {
        struct uffdio_api api = { .api = UFFD_API, .features = UFFD_FEATURE_WP_ASYNC };
        if (ioctl(uffd, UFFDIO_API, &api) == 0 && (api.features & UFFD_FEATURE_WP_ASYNC)) {
            struct uffdio_register reg = {
                .range = { .start = heap_base, .len = EXTRA_TRACK_HEAP_LIMIT },
                .mode = UFFDIO_REGISTER_MODE_WP
            };
            if (ioctl(uffd, UFFDIO_REGISTER, &reg) == 0) {
                struct uffdio_writeprotect wp = {
                    .range = { .start = heap_base, .len = EXTRA_TRACK_HEAP_LIMIT },
                    .mode = UFFDIO_WRITEPROTECT_MODE_WP
                };
                ioctl(uffd, UFFDIO_WRITEPROTECT, &wp);
                int pm = open("/proc/self/pagemap", O_RDONLY | O_CLOEXEC);
                if (pm >= 0) {
                    g_extra_uffd_fd = uffd;
                    g_extra_pagemap_fd = pm;
                    g_extra_track_mode = EXTRA_TRACK_MODE_UFFD_WP;
                    fprintf(stderr, "[extra] Page write tracking: userfaultfd async WP active (heap 0x%llx)\n",
                            (unsigned long long)heap_base);
                    return;
                }
            }
        }
        close(uffd);
    }

    /* 2. Fallback to soft-dirty */
    g_extra_pagemap_fd = open("/proc/self/pagemap", O_RDONLY | O_CLOEXEC);
    g_extra_clear_refs_fd = open("/proc/self/clear_refs", O_WRONLY | O_CLOEXEC);
    if (g_extra_pagemap_fd >= 0 && g_extra_clear_refs_fd >= 0) {
        ssize_t w = write(g_extra_clear_refs_fd, "4\n", 2);
        (void)w;
        g_extra_last_cr_ns = extra_track_time_ns();
        g_extra_track_mode = EXTRA_TRACK_MODE_SOFT_DIRTY;
        fprintf(stderr, "[extra] Page write tracking: soft-dirty fallback active\n");
        return;
    }

    g_extra_track_mode = EXTRA_TRACK_MODE_FAILED;
    fprintf(stderr, "[extra] Page write tracking unavailable\n");
}

/* Scans pages written by CPU in [lo, hi) and stamps track_gen with generation gen. */
static inline void extra_track_harvest(uint64_t lo, uint64_t hi, uint64_t heap_base, uint32_t *track_gen, uint32_t gen) {
    if (lo >= hi || !track_gen) return;
    extra_track_init(heap_base);
    if (g_extra_track_mode == EXTRA_TRACK_MODE_FAILED) return;

    if (g_extra_track_mode == EXTRA_TRACK_MODE_UFFD_WP) {
        struct page_region regions[EXTRA_TRACK_MAX_REGIONS];
        struct pm_scan_arg arg = {0};
        arg.size = sizeof(arg);
        arg.flags = PM_SCAN_WP_MATCHING;
        arg.start = lo;
        arg.end = hi;
        arg.vec = (uint64_t)(uintptr_t)regions;
        arg.vec_len = EXTRA_TRACK_MAX_REGIONS;
        arg.category_mask = PAGE_IS_WRITTEN;
        arg.return_mask = PAGE_IS_WRITTEN;

        while (arg.start < hi) {
            arg.walk_end = 0;
            long ret = ioctl(g_extra_pagemap_fd, PAGEMAP_SCAN, &arg);
            if (ret <= 0) break;
            for (long i = 0; i < ret; i++) {
                uint64_t r_start = regions[i].start < lo ? lo : regions[i].start;
                uint64_t r_end = regions[i].end > hi ? hi : regions[i].end;
                for (uint64_t a = r_start; a < r_end; a += EXTRA_TRACK_PAGE_SIZE) {
                    if (a >= heap_base) {
                        uint64_t idx = (a - heap_base) >> 12;
                        track_gen[idx] = gen;
                    }
                }
            }
            if (arg.walk_end <= arg.start || arg.walk_end >= hi) break;
            arg.start = arg.walk_end;
        }
        return;
    }

    if (g_extra_track_mode == EXTRA_TRACK_MODE_SOFT_DIRTY) {
        struct page_region regions[EXTRA_TRACK_MAX_REGIONS];
        struct pm_scan_arg arg = {0};
        arg.size = sizeof(arg);
        arg.start = lo;
        arg.end = hi;
        arg.vec = (uint64_t)(uintptr_t)regions;
        arg.vec_len = EXTRA_TRACK_MAX_REGIONS;
        arg.category_mask = PAGE_IS_SOFT_DIRTY;
        arg.return_mask = PAGE_IS_SOFT_DIRTY;

        bool found_dirty = false;
        while (arg.start < hi) {
            arg.walk_end = 0;
            long ret = ioctl(g_extra_pagemap_fd, PAGEMAP_SCAN, &arg);
            if (ret < 0) {
                if (errno == EINVAL || errno == ENOTTY) g_extra_track_mode = EXTRA_TRACK_MODE_PREAD;
                break;
            }
            if (ret > 0) found_dirty = true;
            for (long i = 0; i < ret; i++) {
                uint64_t r_start = regions[i].start < lo ? lo : regions[i].start;
                uint64_t r_end = regions[i].end > hi ? hi : regions[i].end;
                for (uint64_t a = r_start; a < r_end; a += EXTRA_TRACK_PAGE_SIZE) {
                    if (a >= heap_base) {
                        uint64_t idx = (a - heap_base) >> 12;
                        track_gen[idx] = gen;
                    }
                }
            }
            if (arg.walk_end <= arg.start || arg.walk_end >= hi) break;
            arg.start = arg.walk_end;
        }

        if (found_dirty && g_extra_clear_refs_fd >= 0) {
            uint64_t now = extra_track_time_ns();
            if (now - g_extra_last_cr_ns >= 16000000ULL) {
                ssize_t w = write(g_extra_clear_refs_fd, "4\n", 2);
                (void)w;
                g_extra_last_cr_ns = now;
            }
        }
        return;
    }

    if (g_extra_track_mode == EXTRA_TRACK_MODE_PREAD) {
        uint64_t entries[512];
        bool found_dirty = false;
        for (uint64_t a = lo; a < hi; ) {
            uint64_t chunk_pages = (hi - a) >> 12;
            if (chunk_pages > 512) chunk_pages = 512;
            off_t offset = (off_t)((a >> 12) * sizeof(uint64_t));
            ssize_t rd = pread(g_extra_pagemap_fd, entries, chunk_pages * sizeof(uint64_t), offset);
            if (rd <= 0) break;
            size_t n = (size_t)rd / sizeof(uint64_t);
            for (size_t i = 0; i < n; i++) {
                if ((entries[i] >> 55) & 1ULL) {
                    found_dirty = true;
                    uint64_t page_addr = a + (i << 12);
                    if (page_addr >= heap_base) {
                        uint64_t idx = (page_addr - heap_base) >> 12;
                        track_gen[idx] = gen;
                    }
                }
            }
            a += n << 12;
        }
        if (found_dirty && g_extra_clear_refs_fd >= 0) {
            uint64_t now = extra_track_time_ns();
            if (now - g_extra_last_cr_ns >= 16000000ULL) {
                ssize_t w = write(g_extra_clear_refs_fd, "4\n", 2);
                (void)w;
                g_extra_last_cr_ns = now;
            }
        }
    }
}
