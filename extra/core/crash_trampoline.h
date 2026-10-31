#pragma once
#include <stdint.h>
#include <stddef.h>

#define WINAPI
#define CALLBACK

typedef struct _EXCEPTION_RECORD {
    uint32_t ExceptionCode;
    uint32_t ExceptionFlags;
    struct _EXCEPTION_RECORD *ExceptionRecord;
    void *ExceptionAddress;
    uint32_t NumberParameters;
    uintptr_t ExceptionInformation[15];
} EXCEPTION_RECORD, *PEXCEPTION_RECORD;

#define CONTEXT_CONTROL 1
typedef struct _CONTEXT {
    uint32_t ContextFlags;
    uint64_t Rip;
    uint64_t Rsp;
} CONTEXT, *PCONTEXT;

typedef struct _EXCEPTION_POINTERS {
    PEXCEPTION_RECORD ExceptionRecord;
    PCONTEXT ContextRecord;
} EXCEPTION_POINTERS, *PEXCEPTION_POINTERS;

typedef int32_t (CALLBACK *PTOP_LEVEL_EXCEPTION_FILTER)(PEXCEPTION_POINTERS);
typedef PTOP_LEVEL_EXCEPTION_FILTER LPTOP_LEVEL_EXCEPTION_FILTER;
static inline LPTOP_LEVEL_EXCEPTION_FILTER SetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER f) { (void)f; return NULL; }

typedef uint32_t MINIDUMP_TYPE;
#define MiniDumpWithThreadInfo 0
#define MiniDumpWithUnloadedModules 0

typedef struct _MINIDUMP_EXCEPTION_INFORMATION {
    uint32_t ThreadId;
    PEXCEPTION_POINTERS ExceptionPointers;
    int ClientPointers;
} MINIDUMP_EXCEPTION_INFORMATION, *PMINIDUMP_EXCEPTION_INFORMATION;

typedef void *PMINIDUMP_USER_STREAM_INFORMATION;
typedef void *PMINIDUMP_CALLBACK_INFORMATION;
