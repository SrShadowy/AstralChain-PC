#pragma once // [extra]
/* Horizon OS architectural definitions: Supervisor Calls (SVC), Handle ranges,
 * Kernel Result codes, Memory Types/Permissions, GetInfo queries and CMIF protocols. */

#include <stdint.h>

/* Horizon OS Supervisor Call (SVC) immediate IDs */
enum HorizonSvc {
    SVC_SET_HEAP_SIZE               = 0x01,
    SVC_SET_MEMORY_ATTRIBUTE        = 0x03,
    SVC_MAP_MEMORY                  = 0x04,
    SVC_UNMAP_MEMORY                = 0x05,
    SVC_QUERY_MEMORY                = 0x06,
    SVC_CREATE_THREAD               = 0x08,
    SVC_START_THREAD                = 0x09,
    SVC_EXIT_THREAD                 = 0x0A,
    SVC_SLEEP_THREAD                = 0x0B,
    SVC_GET_THREAD_PRIORITY         = 0x0C,
    SVC_SET_THREAD_PRIORITY         = 0x0D,
    SVC_GET_THREAD_CORE_MASK        = 0x0E,
    SVC_SET_THREAD_CORE_MASK        = 0x0F,
    SVC_GET_CURRENT_PROCESSOR       = 0x10,
    SVC_MAP_SHARED_MEMORY           = 0x13,
    SVC_UNMAP_SHARED_MEMORY         = 0x14,
    SVC_CREATE_TRANSFER_MEMORY      = 0x15,
    SVC_CLOSE_HANDLE                = 0x16,
    SVC_RESET_SIGNAL                = 0x17,
    SVC_WAIT_SYNCHRONIZATION        = 0x18,
    SVC_ARBITRATE_LOCK              = 0x1A,
    SVC_ARBITRATE_UNLOCK            = 0x1B,
    SVC_WAIT_PROCESS_WIDE_KEY       = 0x1C,
    SVC_SIGNAL_PROCESS_WIDE_KEY     = 0x1D,
    SVC_GET_SYSTEM_TICK             = 0x1E,
    SVC_CONNECT_TO_NAMED_PORT       = 0x1F,
    SVC_SEND_SYNC_REQUEST           = 0x21,
    SVC_GET_THREAD_ID               = 0x25,
    SVC_GET_INFO                    = 0x29,
    SVC_WAIT_FOR_ADDRESS            = 0x34,
    SVC_SIGNAL_TO_ADDRESS           = 0x35,

    /* Custom runtime patches (Dynamic-FPS / Cutscene frame pacing) */
    SVC_CUSTOM_CUTSCENE_PACE        = 0xB1,
    SVC_CUSTOM_CUTSCENE_CARRY       = 0xB2,
};

/* Horizon OS Handle allocation partitions & pseudo-handles */
enum HorizonHandleRange {
    HANDLE_CURRENT_THREAD           = 0xFFFF8000u,
    HANDLE_CURRENT_PROCESS          = 0xFFFF8001u,

    HANDLE_MAIN_THREAD              = 0x1000,

    HANDLE_SESSION_BASE             = 0x2000,
    HANDLE_SESSION_COUNT            = 64,
    HANDLE_SESSION_LIMIT            = (HANDLE_SESSION_BASE + HANDLE_SESSION_COUNT), /* 0x2040 */

    HANDLE_EVENT_BASE               = 0x3000,
    HANDLE_EVENT_COUNT              = 64,
    HANDLE_EVENT_LIMIT              = (HANDLE_EVENT_BASE + HANDLE_EVENT_COUNT),     /* 0x3040 */

    HANDLE_SHARED_BASE              = 0x4000,
    HANDLE_SHARED_COUNT             = 64,
    HANDLE_SHARED_LIMIT             = (HANDLE_SHARED_BASE + HANDLE_SHARED_COUNT),    /* 0x4040 */

    HANDLE_THREAD_BASE              = 0x5000,
    HANDLE_THREAD_COUNT             = 32,
    HANDLE_THREAD_LIMIT             = (HANDLE_THREAD_BASE + HANDLE_THREAD_COUNT),    /* 0x5020 */

    HANDLE_TRANSFER_BASE            = 0x6000,
    HANDLE_TRANSFER_COUNT           = 32,
    HANDLE_TRANSFER_LIMIT           = (HANDLE_TRANSFER_BASE + HANDLE_TRANSFER_COUNT),/* 0x6020 */
};

/* Horizon OS Kernel Result / Error codes (Module + Description) */
enum HorizonResult {
    RESULT_SUCCESS                  = 0x0000,
    RESULT_NO_MESSAGE               = 0x0680, /* Common state / applet receive empty */
    RESULT_INVALID_SIZE             = 0xCA01, /* OS kernel: Invalid size (0x101) */
    RESULT_INVALID_ADDRESS          = 0xCC01, /* OS kernel: Invalid memory address (0x102) */
    RESULT_OUT_OF_MEMORY            = 0xD001, /* OS kernel: Out of memory / resource (0x104) */
    RESULT_INVALID_CURRENT_MEMORY   = 0xD401, /* OS kernel: Invalid memory state / permission (0x106) */
    RESULT_TIMEOUT                  = 0xEA01, /* OS kernel: Wait timeout (0x117) */
    RESULT_INTERRUPTED              = 0xEE01, /* OS kernel: Synchronization interrupted (0x119) */
    RESULT_PORT_CLOSED              = 0xF601, /* OS kernel: Session peer connection closed (0x123) */
};

/* Horizon OS GetInfo (SVC 0x29) query sub-types */
enum HorizonInfoType {
    INFO_CORE_MASK                      = 0,
    INFO_PRIORITY_MASK                  = 1,
    INFO_ALIAS_REGION_ADDRESS           = 2,
    INFO_ALIAS_REGION_SIZE              = 3,
    INFO_HEAP_REGION_ADDRESS            = 4,
    INFO_HEAP_REGION_SIZE               = 5,
    INFO_TOTAL_MEMORY_SIZE              = 6,
    INFO_USED_MEMORY_SIZE               = 7,
    INFO_RANDOM_ENTROPY                 = 11,
    INFO_ASLR_REGION_ADDRESS            = 12,
    INFO_ASLR_REGION_SIZE               = 13,
    INFO_STACK_REGION_ADDRESS           = 14,
    INFO_STACK_REGION_SIZE              = 15,
    INFO_SYSTEM_RESOURCE_SIZE           = 16,
    INFO_SYSTEM_RESOURCE_USED           = 17,
    INFO_INITIAL_PROCESS_ID_RANGE       = 21,
    INFO_USER_EXCEPTION_CONTEXT_ADDRESS = 22,
};

/* Horizon OS Memory Permissions (MemoryInfo.perm) */
enum HorizonMemoryPermission {
    PERM_NONE       = 0,
    PERM_R          = 1,
    PERM_W          = 2,
    PERM_RW         = 3,
    PERM_X          = 4,
    PERM_RX         = 5,
};

/* Horizon OS Memory Types (MemoryInfo.type) */
enum HorizonMemoryType {
    MEM_TYPE_UNMAPPED       = 0,
    MEM_TYPE_IO             = 1,
    MEM_TYPE_NORMAL         = 2,
    MEM_TYPE_CODE_STATIC    = 3,
    MEM_TYPE_CODE_MUTABLE   = 4,
    MEM_TYPE_HEAP           = 5,
    MEM_TYPE_SHARED         = 6,
    MEM_TYPE_WEAK_CODE      = 7,
    MEM_TYPE_STACK          = 11,
    MEM_TYPE_THREAD_LOCAL   = 12,
    MEM_TYPE_TRANSFER       = 13,
    MEM_TYPE_RESERVED       = 16,
};

/* Horizon OS CMIF / HIPC Request Protocol Types */
enum CmifRequestType {
    CMIF_REQUEST_INVALID            = 0,
    CMIF_REQUEST_LEGACY_REQUEST     = 1,
    CMIF_REQUEST_CLOSE              = 2,
    CMIF_REQUEST_LEGACY_CONTROL     = 3,
    CMIF_REQUEST_COMMAND            = 4,
    CMIF_REQUEST_CONTROL            = 5,
    CMIF_REQUEST_COMMAND_WITH_CTX   = 6,
    CMIF_REQUEST_CONTROL_WITH_CTX   = 7,
};

/* Horizon OS CMIF Domain Command Types */
enum CmifDomainCommand {
    CMIF_DOMAIN_SEND_MESSAGE        = 1,
    CMIF_DOMAIN_CLOSE_OBJECT        = 2,
};

/* Horizon OS Library Applet IDs */
enum HorizonAppletId {
    APPLET_ID_CONTROLLER            = 0x0C,
    APPLET_ID_SOFTWARE_KEYBOARD     = 0x11,
};

/* Horizon OS APM Performance Configurations */
enum HorizonApmConfig {
    APM_PERF_MODE_HANDHELD          = 0,
    APM_PERF_MODE_DOCKED            = 1,
    APM_CONFIG_HANDHELD             = 0x20004,
    APM_CONFIG_DOCKED               = 0x10001,
};

/* Horizon OS HID Controller Style Masks */
enum HorizonNpadStyle {
    NPAD_STYLE_SUPPORT_ALL          = 0x7F,
};

/* Horizon OS CMIF Header Magic Signatures */
#define CMIF_IN_HEADER_MAGIC        0x49434653u /* 'SFCI' */
#define CMIF_OUT_HEADER_MAGIC       0x4f434653u /* 'SFCO' */

/* Virtual Memory & Alignment Constants */
#define HORIZON_PAGE_SIZE               4096ull
#define HORIZON_PAGE_MASK               4095ull
#define HORIZON_HEAP_ALIGNMENT          0x200000ull     /* 2 MB huge page alignment */
#define HORIZON_ADDRESS_SPACE_LIMIT     (1ull << 39)    /* 512 GB ARM64 virtual address space */

/* Virtual Memory Region Map (Horizon GetInfo type 2, 3, 15) */
#define HORIZON_ALIAS_REGION_ADDRESS    0x4000000000ull /* 256 GB base address */
#define HORIZON_ALIAS_REGION_SIZE       0x1000000000ull /* 64 GB region size */
#define HORIZON_STACK_REGION_SIZE       0x100000000ull  /* 4 GB stack reservation */

/* Tegra X1 / ARM Cortex-A57 generic system counter frequency (CNTFRQ_EL0).
 * Nintendo Switch hardware increments the system tick at exactly 19.2 MHz (1 tick ~= 52.083 ns). */
#define HORIZON_SYSTEM_TICK_FREQ        19200000.0
#define HORIZON_SYSTEM_TICK_FREQ_U64    19200000ull

/* Convert Horizon system ticks to fractional seconds (wall-clock time) */
static inline double horizon_ticks_to_seconds(uint64_t ticks) {
    return (double)ticks / HORIZON_SYSTEM_TICK_FREQ;
}

/* Cutscene Timing & Frame Pacing Constants:
 * Astral Chain cutscenes are rendered and animated at a fixed base cadence of 30 FPS.
 * Dynamic-FPS hooks custom SVCs (0xB1 / 0xB2) into the game loop to pace animation
 * tracks, compensate for dropped frames, and synchronize A/V playback. */
#define CUTSCENE_BASE_FPS               30.0f
#define CUTSCENE_FRAME_MS_BASE          (1000.0f / CUTSCENE_BASE_FPS) /* ~33.333 ms per frame */
#define CUTSCENE_DEFAULT_STEP           1.0f    /* Normal progression: 1 frame step at 30 FPS */
#define CUTSCENE_MAX_MEASURED_STEP      30.0f   /* Maximum clamp on measured step delta (1 second of frames) */
#define CUTSCENE_STALL_THRESHOLD_STEPS  6.0f    /* 6 frames at 30 FPS (~200 ms: audible A/V desync threshold) */
#define CUTSCENE_MAX_SANITY_STEPS       9000.0f /* 9000 frames at 30 FPS (~300 s / 5 min: filters loadings/pauses) */
#define CUTSCENE_MAX_CATCHUP_PER_FRAME  7.0f    /* Maximum extra frames compensated per tick during lag recovery */
#define CUTSCENE_MIN_DEBT_LIMIT         (-2.0f) /* Minimum pacing debt before clamping */
#define CUTSCENE_TRACK_SLOT_COUNT       64      /* Table size for audio/animation track state */

/* Kernel diagnostics */
#define SVC_LOCK_WARNING_THRESHOLD_MS   30.0    /* Warning threshold in ms for long SVC locks */
// [extra]

