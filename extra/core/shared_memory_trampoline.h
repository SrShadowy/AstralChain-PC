#pragma once
#define _GNU_SOURCE
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "sync_trampoline.h"

#ifndef SYS_memfd_create
#if defined(__x86_64__)
#define SYS_memfd_create 319
#elif defined(__aarch64__)
#define SYS_memfd_create 279
#elif defined(__riscv)
#define SYS_memfd_create 279
#endif
#endif

#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif

#ifndef FILE_MAP_WRITE
#define FILE_MAP_WRITE 0x0002
#endif
#ifndef FILE_MAP_READ
#define FILE_MAP_READ 0x0004
#endif
#ifndef FILE_MAP_ALL_ACCESS
#define FILE_MAP_ALL_ACCESS 0x000F001F
#endif

#ifndef PAGE_NOACCESS
#define PAGE_NOACCESS 0x01
#endif
#ifndef PAGE_READONLY
#define PAGE_READONLY 0x02
#endif
#ifndef PAGE_READWRITE
#define PAGE_READWRITE 0x04
#endif

static inline HANDLE CreateFileMappingW(HANDLE hFile, void *sec, uint32_t prot, uint32_t high, uint32_t low, const wchar_t *name) {
    (void)sec; (void)prot; (void)name;
    size_t sz = ((size_t)high << 32) | (size_t)low;
    if (!sz) sz = 4096;
    int fd = -1;
    if (hFile != (HANDLE)(intptr_t)-1 && hFile && hFile->type == EXTRA_HANDLE_FILE) {
        fd = dup(hFile->file.fd);
    } else {
#if defined(SYS_memfd_create)
        fd = (int)syscall(SYS_memfd_create, "ac_shared_mem", MFD_CLOEXEC);
#endif
        if (fd >= 0) {
            if (ftruncate(fd, (off_t)sz) != 0) {
                close(fd);
                return NULL;
            }
        }
    }
    if (fd < 0) return NULL;
    ExtraHandle *h = (ExtraHandle *)calloc(1, sizeof(ExtraHandle));
    if (!h) {
        close(fd);
        return NULL;
    }
    h->type = EXTRA_HANDLE_MAPPING;
    h->ref_count = 1;
    h->mapping.fd = fd;
    h->mapping.size = sz;
    return (HANDLE)h;
}

static inline void *MapViewOfFile(HANDLE h, uint32_t access, uint32_t hi, uint32_t lo, size_t sz) {
    (void)hi;
    if (!h || h->type != EXTRA_HANDLE_MAPPING) return NULL;
    size_t map_sz = sz ? sz : h->mapping.size;
    off_t offset = (off_t)lo;
    int prot = (access == FILE_MAP_READ) ? PROT_READ : (PROT_READ | PROT_WRITE);
    void *p = mmap(NULL, map_sz, prot, MAP_SHARED, h->mapping.fd, offset);
    return (p == MAP_FAILED) ? NULL : p;
}

static inline int UnmapViewOfFile(const void *p) {
    (void)p;
    return 1;
}

static inline void *extra_posix_virtual_alloc2(HANDLE h, void *b, size_t s, uint32_t t, uint32_t p, void *x, uint32_t c) {
    (void)h; (void)t; (void)p; (void)x; (void)c;
    void *ptr = mmap(b, s, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED, -1, 0);
    return (ptr == MAP_FAILED) ? NULL : ptr;
}

static inline void *extra_posix_map_view_of_file3(HANDLE h, HANDLE p, void *b, uint64_t o, size_t s, uint32_t t, uint32_t pr, void *x, uint32_t c) {
    (void)p; (void)t; (void)x; (void)c;
    if (!h || h->type != EXTRA_HANDLE_MAPPING) return NULL;
    int prot = (pr == PAGE_READONLY) ? PROT_READ : (PROT_READ | PROT_WRITE);
    void *mapped = mmap(b, s, prot, MAP_SHARED | MAP_FIXED, h->mapping.fd, (off_t)o);
    return (mapped == MAP_FAILED) ? NULL : mapped;
}

static inline int extra_posix_unmap_view_of_file2(HANDLE hp, void *base, uint32_t flags) {
    (void)hp; (void)flags;
    return mmap(base, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED, -1, 0) != MAP_FAILED;
}
