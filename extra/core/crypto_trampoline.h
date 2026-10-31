#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef void *BCRYPT_ALG_HANDLE;
typedef int32_t NTSTATUS;
#define BCRYPT_USE_SYSTEM_PREFERRED_RNG 0x00000002
#define BCRYPT_SHA256_ALGORITHM L"SHA256"

static inline int BCryptGenRandom(void *h, unsigned char *pb, unsigned long cb, unsigned long dw) {
    (void)h; (void)dw;
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        size_t rd = fread(pb, 1, cb, f);
        fclose(f);
        if (rd == cb) return 0;
    }
    for (unsigned long i = 0; i < cb; i++) pb[i] = (unsigned char)rand();
    return 0;
}

#define BCryptOpenAlgorithmProvider(phAlg, pszAlgId, pszImplementation, dwFlags) (1)
#define BCryptHash(hAlgorithm, pbSecret, cbSecret, pbInput, cbInput, pbOutput, cbOutput) (1)
#define BCryptCloseAlgorithmProvider(hAlgorithm, dwFlags) (0)
