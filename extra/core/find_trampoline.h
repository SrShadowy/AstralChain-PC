#pragma once
#define _GNU_SOURCE
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include "platform_posix.h"
#include "sync_trampoline.h"

typedef struct _WIN32_FIND_DATAA {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime;
    DWORD nFileSizeHigh, nFileSizeLow, dwReserved0, dwReserved1;
    char cFileName[MAX_PATH];
    char cAlternateFileName[14];
} WIN32_FIND_DATAA, *PWIN32_FIND_DATAA, *LPWIN32_FIND_DATAA;

typedef struct _WIN32_FIND_DATAW {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime;
    DWORD nFileSizeHigh, nFileSizeLow, dwReserved0, dwReserved1;
    wchar_t cFileName[MAX_PATH];
    wchar_t cAlternateFileName[14];
} WIN32_FIND_DATAW, *PWIN32_FIND_DATAW, *LPWIN32_FIND_DATAW;

static inline int extra_find_match(const char *pat, const char *name) {
    if (!pat || !*pat || !strcmp(pat, "*") || !strcmp(pat, "*.*")) return 1;
    if (pat[0] == '*' && pat[1] == '.') {
        const char *ext = strrchr(name, '.');
        return ext && !strcmp(ext, pat + 1);
    }
    return !strcmp(pat, name);
}

static inline BOOL extra_fill_find_data_a(const char *dir_path, const char *entry_name, WIN32_FIND_DATAA *data) {
    if (!entry_name || !data) return FALSE;
    memset(data, 0, sizeof(*data));
    strncpy(data->cFileName, entry_name, MAX_PATH - 1);
    char full[4096];
    snprintf(full, sizeof(full), "%s/%s", dir_path, entry_name);
    struct stat st;
    if (stat(full, &st) == 0) {
        data->dwFileAttributes = S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
        data->nFileSizeLow = (DWORD)st.st_size;
        data->nFileSizeHigh = (DWORD)((uint64_t)st.st_size >> 32);
    } else {
        data->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    }
    return TRUE;
}

static inline BOOL extra_fill_find_data_w(const char *dir_path, const char *entry_name, WIN32_FIND_DATAW *data) {
    if (!entry_name || !data) return FALSE;
    memset(data, 0, sizeof(*data));
    for (int i = 0; entry_name[i] && i < MAX_PATH - 1; i++) data->cFileName[i] = (wchar_t)(unsigned char)entry_name[i];
    char full[4096];
    snprintf(full, sizeof(full), "%s/%s", dir_path, entry_name);
    struct stat st;
    if (stat(full, &st) == 0) {
        data->dwFileAttributes = S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
        data->nFileSizeLow = (DWORD)st.st_size;
        data->nFileSizeHigh = (DWORD)((uint64_t)st.st_size >> 32);
    } else {
        data->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    }
    return TRUE;
}

static inline HANDLE FindFirstFileA(const char *lpFileName, WIN32_FIND_DATAA *lpFindFileData) {
    char path[4096];
    for (int i = 0; lpFileName[i] && i < 4095; i++) path[i] = (lpFileName[i] == '\\') ? '/' : lpFileName[i];
    path[strlen(lpFileName) < 4095 ? strlen(lpFileName) : 4095] = '\0';
    char pat[64] = "*";
    char *slash = strrchr(path, '/');
    if (slash) {
        strncpy(pat, slash + 1, sizeof(pat) - 1);
        *slash = '\0';
    }
    DIR *d = opendir(path[0] ? path : ".");
    if (!d) { SetLastError(ERROR_PATH_NOT_FOUND); return INVALID_HANDLE_VALUE; }
    struct dirent *ent = NULL;
    while ((ent = readdir(d)) != NULL) {
        if (extra_find_match(pat, ent->d_name)) break;
    }
    if (!ent) { closedir(d); SetLastError(ERROR_FILE_NOT_FOUND); return INVALID_HANDLE_VALUE; }
    ExtraHandle *h = (ExtraHandle *)calloc(1, sizeof(ExtraHandle));
    if (!h) { closedir(d); SetLastError(ERROR_NOT_ENOUGH_MEMORY); return INVALID_HANDLE_VALUE; }
    h->type = EXTRA_HANDLE_FIND;
    h->ref_count = 1;
    h->find.dir = d;
    strncpy(h->find.base_path, path[0] ? path : ".", sizeof(h->find.base_path) - 1);
    strncpy(h->find.pattern, pat, sizeof(h->find.pattern) - 1);
    extra_fill_find_data_a(h->find.base_path, ent->d_name, lpFindFileData);
    SetLastError(0);
    return (HANDLE)h;
}

static inline BOOL FindNextFileA(HANDLE hFindFile, WIN32_FIND_DATAA *lpFindFileData) {
    if (!hFindFile || hFindFile == INVALID_HANDLE_VALUE || hFindFile->type != EXTRA_HANDLE_FIND) return FALSE;
    DIR *d = (DIR *)hFindFile->find.dir;
    if (!d) return FALSE;
    struct dirent *ent = NULL;
    while ((ent = readdir(d)) != NULL) {
        if (extra_find_match(hFindFile->find.pattern, ent->d_name)) break;
    }
    if (!ent) { SetLastError(ERROR_NO_MORE_FILES); return FALSE; }
    SetLastError(0);
    return extra_fill_find_data_a(hFindFile->find.base_path, ent->d_name, lpFindFileData);
}

static inline HANDLE FindFirstFileW(const wchar_t *lpFileName, WIN32_FIND_DATAW *lpFindFileData) {
    char path[4096];
    for (int i = 0; lpFileName[i] && i < 4095; i++) path[i] = (lpFileName[i] == L'\\') ? '/' : (char)lpFileName[i];
    path[wcslen(lpFileName) < 4095 ? wcslen(lpFileName) : 4095] = '\0';
    char pat[64] = "*";
    char *slash = strrchr(path, '/');
    if (slash) {
        strncpy(pat, slash + 1, sizeof(pat) - 1);
        *slash = '\0';
    }
    DIR *d = opendir(path[0] ? path : ".");
    if (!d) { SetLastError(ERROR_PATH_NOT_FOUND); return INVALID_HANDLE_VALUE; }
    struct dirent *ent = NULL;
    while ((ent = readdir(d)) != NULL) {
        if (extra_find_match(pat, ent->d_name)) break;
    }
    if (!ent) { closedir(d); SetLastError(ERROR_FILE_NOT_FOUND); return INVALID_HANDLE_VALUE; }
    ExtraHandle *h = (ExtraHandle *)calloc(1, sizeof(ExtraHandle));
    if (!h) { closedir(d); SetLastError(ERROR_NOT_ENOUGH_MEMORY); return INVALID_HANDLE_VALUE; }
    h->type = EXTRA_HANDLE_FIND;
    h->ref_count = 1;
    h->find.dir = d;
    strncpy(h->find.base_path, path[0] ? path : ".", sizeof(h->find.base_path) - 1);
    strncpy(h->find.pattern, pat, sizeof(h->find.pattern) - 1);
    extra_fill_find_data_w(h->find.base_path, ent->d_name, lpFindFileData);
    SetLastError(0);
    return (HANDLE)h;
}

static inline BOOL FindNextFileW(HANDLE hFindFile, WIN32_FIND_DATAW *lpFindFileData) {
    if (!hFindFile || hFindFile == INVALID_HANDLE_VALUE || hFindFile->type != EXTRA_HANDLE_FIND) return FALSE;
    DIR *d = (DIR *)hFindFile->find.dir;
    if (!d) return FALSE;
    struct dirent *ent = NULL;
    while ((ent = readdir(d)) != NULL) {
        if (extra_find_match(hFindFile->find.pattern, ent->d_name)) break;
    }
    if (!ent) { SetLastError(ERROR_NO_MORE_FILES); return FALSE; }
    SetLastError(0);
    return extra_fill_find_data_w(hFindFile->find.base_path, ent->d_name, lpFindFileData);
}

static inline BOOL FindClose(HANDLE hFindFile) {
    if (!hFindFile || hFindFile == INVALID_HANDLE_VALUE || hFindFile->type != EXTRA_HANDLE_FIND) return FALSE;
    return extra_close_handle(hFindFile);
}
