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

#define MAX_PATH 4096

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

#define ERROR_INVALID_FUNCTION 1
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_PATH_NOT_FOUND 3
#define ERROR_TOO_MANY_OPEN_FILES 4
#define ERROR_ACCESS_DENIED 5
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

#define EXTRA_HIGH_DWORD_SHIFT 32

static inline DWORD errno_to_win32(int err) {
    switch (err) {
        case 0:
            return 0;
        case ENOENT:
            return ERROR_FILE_NOT_FOUND;
        case EEXIST:
            return ERROR_ALREADY_EXISTS;
        case EACCES:
        case EPERM:
            return ERROR_ACCESS_DENIED;
        case ENOSPC:
            return ERROR_DISK_FULL;
        case ENOTEMPTY:
            return ERROR_DIR_NOT_EMPTY;
        case EMFILE:
        case ENFILE:
            return ERROR_TOO_MANY_OPEN_FILES;
        default:
            return ERROR_INVALID_FUNCTION;
    }
}

static inline void extra_path_to_posix(const wchar_t *wpath, char *out, size_t max) {
    if (!wpath || !out || !max) {
        return;
    }
    size_t i = 0;
    while (wpath[i] && i + 1 < max) {
        wchar_t c = wpath[i];
        out[i] = (c == L'\\') ? '/' : (char)c;
        i++;
    }
    out[i] = '\0';
}

static inline void extra_norm_path(char *out, const char *src, size_t max) {
    if (!src || !out || !max) {
        return;
    }
    size_t i = 0;
    while (src[i] && i + 1 < max) {
        out[i] = (src[i] == '\\') ? '/' : src[i];
        i++;
    }
    out[i] = '\0';
}

#include "hooks/storage/dir_trampoline.h"

static inline HANDLE CreateFileW(const wchar_t *lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, void *lpSec, DWORD dwDisp, DWORD dwFlags, HANDLE hTmpl) {
    (void)dwShareMode;
    (void)lpSec;
    (void)dwFlags;
    (void)hTmpl;
    char path[MAX_PATH];
    extra_path_to_posix(lpFileName, path, sizeof(path));
    int flags = 0;
    if ((dwDesiredAccess & GENERIC_READ) && (dwDesiredAccess & GENERIC_WRITE)) {
        flags = O_RDWR;
    } else if (dwDesiredAccess & GENERIC_WRITE) {
        flags = O_WRONLY;
    } else {
        flags = O_RDONLY;
    }
    if (dwDisp == CREATE_NEW) {
        flags |= O_CREAT | O_EXCL;
    } else if (dwDisp == CREATE_ALWAYS) {
        flags |= O_CREAT | O_TRUNC;
    } else if (dwDisp == OPEN_ALWAYS) {
        flags |= O_CREAT;
    }
    int fd = open(path, flags, EXTRA_FILE_MODE_DEFAULT);
    if (fd < 0) {
        SetLastError(errno_to_win32(errno));
        return INVALID_HANDLE_VALUE;
    }
    SetLastError(0);
    ExtraHandle *h = (ExtraHandle *)calloc(1, sizeof(ExtraHandle));
    if (!h) {
        close(fd);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return INVALID_HANDLE_VALUE;
    }
    h->type = EXTRA_HANDLE_FILE;
    h->ref_count = 1;
    h->file.fd = fd;
    return (HANDLE)h;
}

static inline BOOL ReadFile(HANDLE h, void *buf, DWORD nBytes, DWORD *readBytes, void *ov) {
    (void)ov;
    if (!h || h == INVALID_HANDLE_VALUE || h->type != EXTRA_HANDLE_FILE) {
        return FALSE;
    }
    ssize_t n = read(h->file.fd, buf, nBytes);
    if (n < 0) {
        return FALSE;
    }
    if (readBytes) {
        *readBytes = (DWORD)n;
    }
    return TRUE;
}

static inline BOOL WriteFile(HANDLE h, const void *buf, DWORD nBytes, DWORD *writtenBytes, void *ov) {
    (void)ov;
    if (!h || h == INVALID_HANDLE_VALUE || h->type != EXTRA_HANDLE_FILE) {
        return FALSE;
    }
    ssize_t n = write(h->file.fd, buf, nBytes);
    if (n < 0) {
        return FALSE;
    }
    if (writtenBytes) {
        *writtenBytes = (DWORD)n;
    }
    return TRUE;
}

static inline BOOL SetFilePointerEx(HANDLE h, LARGE_INTEGER dist, LARGE_INTEGER *newPos, DWORD method) {
    if (!h || h == INVALID_HANDLE_VALUE || h->type != EXTRA_HANDLE_FILE) {
        return FALSE;
    }
    int whence = SEEK_SET;
    if (method == FILE_CURRENT) {
        whence = SEEK_CUR;
    } else if (method == FILE_END) {
        whence = SEEK_END;
    }
    off_t res = lseek(h->file.fd, (off_t)dist.QuadPart, whence);
    if (res == (off_t)-1) {
        return FALSE;
    }
    if (newPos) {
        newPos->QuadPart = (int64_t)res;
    }
    return TRUE;
}

static inline BOOL GetFileSizeEx(HANDLE h, LARGE_INTEGER *size) {
    if (!h || h == INVALID_HANDLE_VALUE || h->type != EXTRA_HANDLE_FILE || !size) {
        return FALSE;
    }
    struct stat st;
    if (fstat(h->file.fd, &st) != 0) {
        return FALSE;
    }
    size->QuadPart = (int64_t)st.st_size;
    return TRUE;
}

typedef struct _BY_HANDLE_FILE_INFORMATION {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD dwVolumeSerialNumber;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD nNumberOfLinks;
    DWORD nFileIndexHigh;
    DWORD nFileIndexLow;
} BY_HANDLE_FILE_INFORMATION, *PBY_HANDLE_FILE_INFORMATION, *LPBY_HANDLE_FILE_INFORMATION;

static inline BOOL GetFileInformationByHandle(HANDLE hFile, LPBY_HANDLE_FILE_INFORMATION lpInfo) {
    if (!hFile || hFile == INVALID_HANDLE_VALUE || !lpInfo) {
        return FALSE;
    }
    memset(lpInfo, 0, sizeof(*lpInfo));
    if (hFile->type == EXTRA_HANDLE_FILE) {
        struct stat st;
        if (fstat(hFile->file.fd, &st) != 0) {
            return FALSE;
        }
        lpInfo->dwFileAttributes = S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
        lpInfo->nFileSizeLow = (DWORD)st.st_size;
        lpInfo->nFileSizeHigh = (DWORD)((uint64_t)st.st_size >> EXTRA_HIGH_DWORD_SHIFT);
        return TRUE;
    }
    return FALSE;
}

static inline BOOL SetEndOfFile(HANDLE h) {
    if (!h || h == INVALID_HANDLE_VALUE || h->type != EXTRA_HANDLE_FILE) {
        return FALSE;
    }
    off_t cur = lseek(h->file.fd, 0, SEEK_CUR);
    if (cur == (off_t)-1) {
        return FALSE;
    }
    return ftruncate(h->file.fd, cur) == 0;
}

static inline BOOL FlushFileBuffers(HANDLE h) {
    if (!h || h == INVALID_HANDLE_VALUE || h->type != EXTRA_HANDLE_FILE) {
        return FALSE;
    }
    return fsync(h->file.fd) == 0;
}
