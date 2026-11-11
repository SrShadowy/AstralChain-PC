#pragma once
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include "hooks/platform/platform_posix.h"

#define EXTRA_DIR_MODE_DEFAULT 0755
#define EXTRA_FILE_MODE_DEFAULT 0644
#define EXTRA_COPY_BUF_SIZE 8192
#define EXTRA_MKDIR_BUF_SIZE 1024

static inline DWORD errno_to_win32(int err);
static inline void extra_path_to_posix(const wchar_t *wpath, char *out, size_t max);
static inline void extra_norm_path(char *out, const char *src, size_t max);

static inline int extra_storage_mkdir(const char *path) {
    if (!path || !*path) {
        return 0;
    }
    char sub[EXTRA_MKDIR_BUF_SIZE];
    size_t len = strlen(path);
    if (len >= sizeof(sub)) {
        return 0;
    }
    memcpy(sub, path, len + 1);
    for (char *p = sub + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(sub, EXTRA_DIR_MODE_DEFAULT);
            *p = '/';
        }
    }
    return mkdir(sub, EXTRA_DIR_MODE_DEFAULT) == 0 || errno == EEXIST;
}

static inline int extra_storage_copy_file(const char *src, const char *dst) {
    int in = open(src, O_RDONLY);
    if (in < 0) {
        return 0;
    }
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, EXTRA_FILE_MODE_DEFAULT);
    if (out < 0) {
        close(in);
        return 0;
    }
    char buf[EXTRA_COPY_BUF_SIZE];
    ssize_t n;
    int ok = 1;
    while ((n = read(in, buf, sizeof(buf))) > 0) {
        if (write(out, buf, (size_t)n) != n) {
            ok = 0;
            break;
        }
    }
    close(in);
    close(out);
    return ok;
}

static inline BOOL DeleteFileW(const wchar_t *lpFileName) {
    char path[MAX_PATH];
    extra_path_to_posix(lpFileName, path, sizeof(path));
    if (unlink(path) == 0) {
        SetLastError(0);
        return TRUE;
    }
    SetLastError(errno_to_win32(errno));
    return FALSE;
}

static inline BOOL CopyFileW(const wchar_t *src, const wchar_t *dst, BOOL fail_if_exists) {
    (void)fail_if_exists;
    char s[MAX_PATH];
    char d[MAX_PATH];
    extra_path_to_posix(src, s, sizeof(s));
    extra_path_to_posix(dst, d, sizeof(d));
    return extra_storage_copy_file(s, d);
}

static inline BOOL MoveFileExW(const wchar_t *src, const wchar_t *dst, DWORD flags) {
    (void)flags;
    char s[MAX_PATH];
    char d[MAX_PATH];
    extra_path_to_posix(src, s, sizeof(s));
    extra_path_to_posix(dst, d, sizeof(d));
    if (rename(s, d) == 0) {
        SetLastError(0);
        return TRUE;
    }
    SetLastError(errno_to_win32(errno));
    return FALSE;
}

static inline BOOL CreateDirectoryW(const wchar_t *path, void *sec) {
    (void)sec;
    char p[MAX_PATH];
    extra_path_to_posix(path, p, sizeof(p));
    struct stat st;
    if (stat(p, &st) == 0 && S_ISDIR(st.st_mode)) {
        SetLastError(ERROR_ALREADY_EXISTS);
        return FALSE;
    }
    if (extra_storage_mkdir(p)) {
        SetLastError(0);
        return TRUE;
    }
    SetLastError(errno_to_win32(errno));
    return FALSE;
}

static inline BOOL RemoveDirectoryW(const wchar_t *path) {
    char p[MAX_PATH];
    extra_path_to_posix(path, p, sizeof(p));
    if (rmdir(p) == 0) {
        SetLastError(0);
        return TRUE;
    }
    SetLastError(errno_to_win32(errno));
    return FALSE;
}

static inline BOOL GetDiskFreeSpaceExW(const wchar_t *dir, ULARGE_INTEGER *free_caller, ULARGE_INTEGER *total, ULARGE_INTEGER *total_free) {
    char path[MAX_PATH];
    extra_path_to_posix(dir, path, sizeof(path));
    struct statvfs s;
    if (statvfs(path, &s) != 0) {
        return FALSE;
    }
    uint64_t avail = (uint64_t)s.f_bavail * s.f_frsize;
    uint64_t tot = (uint64_t)s.f_blocks * s.f_frsize;
    if (free_caller) {
        free_caller->QuadPart = avail;
    }
    if (total) {
        total->QuadPart = tot;
    }
    if (total_free) {
        total_free->QuadPart = avail;
    }
    return TRUE;
}

static inline DWORD GetFileAttributesW(const wchar_t *lpFileName) {
    char p[MAX_PATH];
    extra_path_to_posix(lpFileName, p, sizeof(p));
    struct stat st;
    if (stat(p, &st) != 0) {
        return INVALID_FILE_ATTRIBUTES;
    }
    return S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
}

static inline DWORD GetFileAttributesA(const char *lpFileName) {
    char p[MAX_PATH];
    extra_norm_path(p, lpFileName, sizeof(p));
    struct stat st;
    if (stat(p, &st) != 0) {
        return INVALID_FILE_ATTRIBUTES;
    }
    return S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
}

static inline BOOL CreateDirectoryA(const char *path, void *sec) {
    (void)sec;
    char p[MAX_PATH];
    extra_norm_path(p, path, sizeof(p));
    return extra_storage_mkdir(p);
}

static inline void extra_storage_walk_files(const char *folder, void (*fn)(const char *path, void *ctx), void *ctx) {
    DIR *d = opendir(folder);
    if (!d) {
        return;
    }
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') {
            continue;
        }
        char sub[MAX_PATH];
        snprintf(sub, sizeof(sub), "%s/%s", folder, de->d_name);
        struct stat st;
        if (stat(sub, &st) == 0 && S_ISDIR(st.st_mode)) {
            extra_storage_walk_files(sub, fn, ctx);
        } else {
            fn(sub, ctx);
        }
    }
    closedir(d);
}
