#pragma once
#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sys/statvfs.h>
#include "platform_posix.h"

#define GENERIC_READ 0x80000000
#define FILE_SHARE_READ 1
#define FILE_SHARE_WRITE 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define CREATE_NEW 1
#define FILE_BEGIN 0
#define FILE_CURRENT 1
#define FILE_END 2
#define FILE_READ_ATTRIBUTES 0x0080
#define FILE_FLAG_OPEN_REPARSE_POINT 0x00200000
#define FILE_FLAG_BACKUP_SEMANTICS 0x02000000
#define FILE_FLAG_WRITE_THROUGH 0x80000000
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define FILE_ATTRIBUTE_REPARSE_POINT 0x0400
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#define MOVEFILE_WRITE_THROUGH 8

#define ERROR_FILE_NOT_FOUND 2
#define ERROR_PATH_NOT_FOUND 3
#define ERROR_TOO_MANY_OPEN_FILES 4
#define ERROR_INVALID_DATA 13
#define ERROR_NO_MORE_FILES 18
#define ERROR_WRITE_FAULT 29
#define ERROR_FILE_EXISTS 80
#define ERROR_INVALID_PARAMETER 87
#define ERROR_DISK_FULL 112
#define ERROR_DIR_NOT_EMPTY 145
#define ERROR_BAD_PATHNAME 161
#define ERROR_ALREADY_EXISTS 183
#define ERROR_INVALID_STATE 5023

static inline DWORD errno_to_win32(int err) {
    switch (err) {
        case 0: return 0;
        case ENOENT: return ERROR_FILE_NOT_FOUND;
        case EEXIST: return ERROR_ALREADY_EXISTS;
        case EACCES: case EPERM: return 5;
        case ENOSPC: return ERROR_DISK_FULL;
        case ENOTEMPTY: return ERROR_DIR_NOT_EMPTY;
        case EMFILE: case ENFILE: return ERROR_TOO_MANY_OPEN_FILES;
        default: return 1;
    }
}

static inline void extra_path_to_posix(const wchar_t *wpath, char *out, size_t max) {
    if (!wpath || !out || !max) return;
    size_t i = 0;
    while (wpath[i] && i + 1 < max) {
        wchar_t c = wpath[i];
        out[i] = (c == L'\\') ? '/' : (char)c;
        i++;
    }
    out[i] = '\0';
}

static inline int extra_storage_mkdir(const char *path) {
    if (!path || !*path) return 0;
    char sub[1024];
    size_t len = strlen(path);
    if (len >= sizeof(sub)) return 0;
    memcpy(sub, path, len + 1);
    for (char *p = sub + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(sub, 0755);
            *p = '/';
        }
    }
    return mkdir(sub, 0755) == 0 || errno == EEXIST;
}

static inline int extra_storage_copy_file(const char *src, const char *dst) {
    int in = open(src, O_RDONLY);
    if (in < 0) return 0;
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) { close(in); return 0; }
    char buf[8192];
    ssize_t n;
    int ok = 1;
    while ((n = read(in, buf, sizeof(buf))) > 0) {
        if (write(out, buf, (size_t)n) != n) { ok = 0; break; }
    }
    close(in);
    close(out);
    return ok;
}

static inline HANDLE CreateFileW(const wchar_t *lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, void *lpSec, DWORD dwDisp, DWORD dwFlags, HANDLE hTmpl) {
    (void)dwShareMode; (void)lpSec; (void)dwFlags; (void)hTmpl;
    char path[4096];
    extra_path_to_posix(lpFileName, path, sizeof(path));
    int flags = 0;
    if ((dwDesiredAccess & GENERIC_READ) && (dwDesiredAccess & GENERIC_WRITE)) flags = O_RDWR;
    else if (dwDesiredAccess & GENERIC_WRITE) flags = O_WRONLY;
    else flags = O_RDONLY;
    if (dwDisp == CREATE_NEW) flags |= O_CREAT | O_EXCL;
    else if (dwDisp == CREATE_ALWAYS) flags |= O_CREAT | O_TRUNC;
    else if (dwDisp == OPEN_ALWAYS) flags |= O_CREAT;
    int fd = open(path, flags, 0644);
    if (fd < 0) {
        SetLastError(errno_to_win32(errno));
        return INVALID_HANDLE_VALUE;
    }
    SetLastError(0);
    ExtraHandle *h = (ExtraHandle *)calloc(1, sizeof(ExtraHandle));
    if (!h) { close(fd); SetLastError(ERROR_NOT_ENOUGH_MEMORY); return INVALID_HANDLE_VALUE; }
    h->type = EXTRA_HANDLE_FILE;
    h->ref_count = 1;
    h->file.fd = fd;
    return (HANDLE)h;
}

static inline BOOL ReadFile(HANDLE h, void *buf, DWORD nBytes, DWORD *readBytes, void *ov) {
    (void)ov;
    if (!h || h == INVALID_HANDLE_VALUE || h->type != EXTRA_HANDLE_FILE) return FALSE;
    ssize_t n = read(h->file.fd, buf, nBytes);
    if (n < 0) return FALSE;
    if (readBytes) *readBytes = (DWORD)n;
    return TRUE;
}

static inline BOOL WriteFile(HANDLE h, const void *buf, DWORD nBytes, DWORD *writtenBytes, void *ov) {
    (void)ov;
    if (!h || h == INVALID_HANDLE_VALUE || h->type != EXTRA_HANDLE_FILE) return FALSE;
    ssize_t n = write(h->file.fd, buf, nBytes);
    if (n < 0) return FALSE;
    if (writtenBytes) *writtenBytes = (DWORD)n;
    return TRUE;
}

static inline BOOL SetFilePointerEx(HANDLE h, LARGE_INTEGER dist, LARGE_INTEGER *newPos, DWORD method) {
    if (!h || h == INVALID_HANDLE_VALUE || h->type != EXTRA_HANDLE_FILE) return FALSE;
    int whence = SEEK_SET;
    if (method == FILE_CURRENT) whence = SEEK_CUR;
    else if (method == FILE_END) whence = SEEK_END;
    off_t res = lseek(h->file.fd, (off_t)dist.QuadPart, whence);
    if (res == (off_t)-1) return FALSE;
    if (newPos) newPos->QuadPart = (int64_t)res;
    return TRUE;
}

static inline BOOL GetFileSizeEx(HANDLE h, LARGE_INTEGER *size) {
    if (!h || h == INVALID_HANDLE_VALUE || h->type != EXTRA_HANDLE_FILE || !size) return FALSE;
    struct stat st;
    if (fstat(h->file.fd, &st) != 0) return FALSE;
    size->QuadPart = (int64_t)st.st_size;
    return TRUE;
}

typedef struct _BY_HANDLE_FILE_INFORMATION {
    DWORD dwFileAttributes; FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime;
    DWORD dwVolumeSerialNumber, nFileSizeHigh, nFileSizeLow, nNumberOfLinks, nFileIndexHigh, nFileIndexLow;
} BY_HANDLE_FILE_INFORMATION, *PBY_HANDLE_FILE_INFORMATION, *LPBY_HANDLE_FILE_INFORMATION;

static inline BOOL GetFileInformationByHandle(HANDLE hFile, LPBY_HANDLE_FILE_INFORMATION lpInfo) {
    if (!hFile || hFile == INVALID_HANDLE_VALUE || !lpInfo) return FALSE;
    memset(lpInfo, 0, sizeof(*lpInfo));
    if (hFile->type == EXTRA_HANDLE_FILE) {
        struct stat st;
        if (fstat(hFile->file.fd, &st) != 0) return FALSE;
        lpInfo->dwFileAttributes = S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
        lpInfo->nFileSizeLow = (DWORD)st.st_size;
        lpInfo->nFileSizeHigh = (DWORD)((uint64_t)st.st_size >> 32);
        return TRUE;
    }
    return FALSE;
}

static inline BOOL SetEndOfFile(HANDLE h) {
    if (!h || h == INVALID_HANDLE_VALUE || h->type != EXTRA_HANDLE_FILE) return FALSE;
    off_t cur = lseek(h->file.fd, 0, SEEK_CUR);
    if (cur == (off_t)-1) return FALSE;
    return ftruncate(h->file.fd, cur) == 0;
}

static inline BOOL FlushFileBuffers(HANDLE h) {
    if (!h || h == INVALID_HANDLE_VALUE || h->type != EXTRA_HANDLE_FILE) return FALSE;
    return fsync(h->file.fd) == 0;
}

static inline BOOL DeleteFileW(const wchar_t *lpFileName) {
    char path[4096];
    extra_path_to_posix(lpFileName, path, sizeof(path));
    if (unlink(path) == 0) { SetLastError(0); return TRUE; }
    SetLastError(errno_to_win32(errno));
    return FALSE;
}

static inline BOOL CopyFileW(const wchar_t *src, const wchar_t *dst, BOOL fail_if_exists) {
    (void)fail_if_exists; char s[4096], d[4096];
    extra_path_to_posix(src, s, sizeof(s)); extra_path_to_posix(dst, d, sizeof(d));
    return extra_storage_copy_file(s, d);
}

static inline BOOL MoveFileExW(const wchar_t *src, const wchar_t *dst, DWORD flags) {
    (void)flags; char s[4096], d[4096];
    extra_path_to_posix(src, s, sizeof(s)); extra_path_to_posix(dst, d, sizeof(d));
    if (rename(s, d) == 0) { SetLastError(0); return TRUE; }
    SetLastError(errno_to_win32(errno));
    return FALSE;
}

static inline BOOL CreateDirectoryW(const wchar_t *path, void *sec) {
    (void)sec; char p[4096]; extra_path_to_posix(path, p, sizeof(p));
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
    char p[4096]; extra_path_to_posix(path, p, sizeof(p));
    if (rmdir(p) == 0) { SetLastError(0); return TRUE; }
    SetLastError(errno_to_win32(errno));
    return FALSE;
}

static inline BOOL GetDiskFreeSpaceExW(const wchar_t *dir, ULARGE_INTEGER *free_caller, ULARGE_INTEGER *total, ULARGE_INTEGER *total_free) {
    char path[4096];
    extra_path_to_posix(dir, path, sizeof(path));
    struct statvfs s;
    if (statvfs(path, &s) != 0) return FALSE;
    uint64_t avail = (uint64_t)s.f_bavail * s.f_frsize;
    uint64_t tot = (uint64_t)s.f_blocks * s.f_frsize;
    if (free_caller) free_caller->QuadPart = avail;
    if (total) total->QuadPart = tot;
    if (total_free) total_free->QuadPart = avail;
    return TRUE;
}

static inline DWORD GetFileAttributesW(const wchar_t *lpFileName) {
    char p[4096];
    extra_path_to_posix(lpFileName, p, sizeof(p));
    struct stat st;
    if (stat(p, &st) != 0) return INVALID_FILE_ATTRIBUTES;
    return S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
}

static inline DWORD GetFileAttributesA(const char *lpFileName) {
    char p[4096];
    for (int i = 0; lpFileName[i] && i < 4095; i++) p[i] = (lpFileName[i] == '\\') ? '/' : lpFileName[i];
    p[strlen(lpFileName) < 4095 ? strlen(lpFileName) : 4095] = '\0';
    struct stat st;
    if (stat(p, &st) != 0) return INVALID_FILE_ATTRIBUTES;
    return S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
}

static inline BOOL CreateDirectoryA(const char *path, void *sec) {
    (void)sec; char p[4096];
    for (int i = 0; path[i] && i < 4095; i++) p[i] = (path[i] == '\\') ? '/' : path[i];
    p[strlen(path) < 4095 ? strlen(path) : 4095] = '\0';
    return extra_storage_mkdir(p);
}

static inline void extra_storage_walk_files(const char *folder, void (*fn)(const char *path, void *ctx), void *ctx) {
    DIR *d = opendir(folder);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char sub[4096];
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
