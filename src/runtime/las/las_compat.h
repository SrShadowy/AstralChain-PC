#pragma once
#ifndef _GNU_SOURCE // [extra]
#define _GNU_SOURCE // [extra]
#endif // [extra]
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32 // [extra]
#include <direct.h> // [extra]
#define mkdir(path, mode) _mkdir(path)
#endif // [extra]
#if defined(_WIN32) || (defined(__GLIBC__) && (__GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ < 38))) // [extra]
static inline size_t strlcpy(char *d, const char *s, size_t n) {
    size_t len = strlen(s);
    if (n) { // [extra]
        size_t k = len < n - 1 ? len : n - 1;
        memcpy(d, s, k);
        d[k] = 0;
    } // [extra]
    return len;
}
#endif // [extra]
#ifndef _WIN32 // [extra]
#include <stdio.h> // [extra]
void _exit(int status) __attribute__((noreturn)); // [extra]
#define _fseeki64 fseeko // [extra]
#endif // [extra]
