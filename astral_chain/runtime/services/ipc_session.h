#pragma once // [extra]
/* Horizon OS IPC session tables, handle allocation, and reply marshalling. */
#include "applet_storage.h"
#include "kernel/horizon_defs.h"

enum SessionKind {
    SESSION_SM                      = 1,
    SESSION_LM                      = 2,
    SESSION_LOGGER                  = 3,
    SESSION_APM                     = 4,
    SESSION_APPLET_OE               = 5,
    SESSION_APPLICATION_PROXY       = 6,
    SESSION_COMMON_STATE            = 7,
    SESSION_SELF_CONTROLLER         = 8,
    SESSION_WINDOW_CONTROLLER       = 9,
    SESSION_AUDIO_CONTROLLER        = 10,
    SESSION_DISPLAY_CONTROLLER      = 11,
    SESSION_LIBRARY_CREATOR         = 12,
    SESSION_APPLICATION_FUNCTIONS   = 13,
    SESSION_DEBUG_FUNCTIONS         = 14,
    SESSION_AOC                     = 15,
    SESSION_PCTL_ROOT               = 16,
    SESSION_PCTL                    = 17,
    SESSION_TIME_ROOT               = 18,
    SESSION_TIME_USER               = 19,
    SESSION_TIME_NETWORK            = 20,
    SESSION_TIME_STEADY             = 21,
    SESSION_TIME_ZONE               = 22,
    SESSION_TIME_LOCAL              = 23,
    SESSION_APM_SESSION             = 24,
    SESSION_SETTINGS                = 25,
    SESSION_HID                     = 26,
    SESSION_HID_RESOURCE            = 27,
    SESSION_CONTROLLER_APPLET       = 28,
    SESSION_STORAGE                 = 29,
    SESSION_STORAGE_ACCESSOR        = 30,
    SESSION_VIBRATION_LIST          = 31,
    SESSION_FS_PROXY                = 32,
    SESSION_ROMFS_STORAGE           = 33,
    SESSION_NVDRV                   = 34,
    SESSION_VI_ROOT                 = 35,
    SESSION_VI_APPLICATION          = 36,
    SESSION_VI_RELAY                = 37,
    SESSION_VI_SYSTEM               = 38,
    SESSION_VI_MANAGER              = 39,
    SESSION_VI_INDIRECT             = 40,
    SESSION_HWOPUS                  = 41,
    SESSION_AUDREN_MANAGER          = 42,
    SESSION_AUDIO_DEVICE            = 43,
    SESSION_AUDOUT_MANAGER          = 44,
    SESSION_AUDIO_OUTPUT            = 45,
    SESSION_NIFM_ROOT               = 46,
    SESSION_NIFM_GENERAL            = 47,
    SESSION_ACCOUNT                 = 48,
    SESSION_ACCOUNT_MANAGER         = 49,
    SESSION_SAVE_FILESYSTEM         = 50,
    SESSION_SAVE_FILE               = 51,
    SESSION_PREPO                   = 52,
};

typedef struct {
    uint32_t revision;
    uint64_t aruid;
} AudioDeviceState;

typedef struct {
    int initialized;
    int restrictions_enabled;
    int paired;
} PctlState;

typedef struct {
    int active;
    int initialized;
    int kind;
    int domain;
    int objects[32];
    unsigned destination;
    PctlState policy;
    PctlState object_policy[32];
    unsigned storage;
    unsigned object_storage[32];
    uint64_t vibration[2];
    uint64_t object_vibration[32][2];
    unsigned nv_transfer;
    uint64_t nv_aruid;
    uint64_t nv_margin_requested;
    unsigned vi_policy;
    unsigned object_vi_policy[32];
    unsigned vi_level;
    unsigned object_vi_level[32];
    unsigned vi_displays;
    unsigned object_vi_displays[32];
    uint32_t account_pid;
    uint32_t object_account_pid[32];
    uint8_t account_user[16];
    uint8_t object_account_user[32][16];
    HANDLE save_file;
    HANDLE object_save_file[32];
    unsigned audio_output;
    unsigned object_audio_output[32];
    AudioDeviceState audio_device;
    AudioDeviceState object_audio_device[32];
} IpcSession;

static IpcSession sessions[HANDLE_SESSION_COUNT];

/* Handle slots and shared server objects have independent lifetimes. */
static unsigned session_handles[HANDLE_SESSION_COUNT];
static unsigned session_refs[HANDLE_SESSION_COUNT];
static unsigned session_peer_closed[HANDLE_SESSION_COUNT];
static size_t log_bytes;

/* Requested console profiles are application state, not Windows clock rates.
 * No default configuration is invented before the application selects one. */
static uint32_t performance_configuration[2];
static unsigned performance_configuration_set[2];

static struct {
    HANDLE handle;
    unsigned access;
} events[HANDLE_EVENT_COUNT];

static HANDLE app_suspend_event;
static HANDLE library_launch_event;
static int library_applet_running;
static int library_applet_created;
static uint32_t library_applet_id;
static HANDLE library_state_event;
static unsigned library_inputs[4];
static unsigned library_input_count;
static unsigned library_output;
static int library_completed;
static HANDLE app_message_event;
static HANDLE addon_change_event;

static unsigned session_index(uint32_t handle) {
    if (handle < HANDLE_SESSION_BASE || handle >= HANDLE_SESSION_LIMIT ||
        !session_handles[handle - HANDLE_SESSION_BASE]) {
        boundary("invalid-session-handle", &state, state.pc);
    }
    return session_handles[handle - HANDLE_SESSION_BASE] - 1;
}

static uint32_t clone_session(unsigned session) {
    for (unsigned h = 0; h < HANDLE_SESSION_COUNT; h++) {
        if (!session_handles[h]) {
            session_handles[h] = session + 1;
            session_peer_closed[h] = 0;
            session_refs[session]++;
            return HANDLE_SESSION_BASE + h;
        }
    }
    boundary("session-handle-limit", &state, state.pc);
}

static uint32_t new_session(int kind) {
    for (unsigned i = 0; i < HANDLE_SESSION_COUNT; i++) {
        if (!session_refs[i]) {
            memset(&sessions[i], 0, sizeof(sessions[i]));
            sessions[i].active = 1;
            sessions[i].initialized = 0;
            sessions[i].kind = kind;
            sessions[i].destination = 1;
            sessions[i].domain = 0;
            memset(sessions[i].objects, 0, sizeof(sessions[i].objects));
            return clone_session(i);
        }
    }
    boundary("session-limit", &state, state.pc);
}

static uint32_t copy_event(HANDLE *server_event) {
    if (!*server_event) {
        *server_event = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!*server_event) {
            boundary("host-event-create", &state, state.pc);
        }
    }
    for (unsigned i = 0; i < HANDLE_EVENT_COUNT; i++) {
        if (!events[i].handle) {
            if (!DuplicateHandle(GetCurrentProcess(), *server_event, GetCurrentProcess(),
                                 &events[i].handle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
                boundary("host-event-copy", &state, state.pc);
            }
            events[i].access = 1;
            return HANDLE_EVENT_BASE + i;
        }
    }
    boundary("event-handle-limit", &state, state.pc);
}

static void close_library_applet(void) {
    if (library_applet_running) {
        boundary("close-running-library-applet", &state, state.pc);
    }
    library_applet_created = 0;
    library_completed = 0;
    if (library_output) {
        storage_release(library_output);
    }
    library_output = 0;
    for (unsigned i = 0; i < library_input_count; i++) {
        storage_release(library_inputs[i]);
    }
    memset(library_inputs, 0, sizeof(library_inputs));
    library_input_count = 0;
    if (library_state_event && !CloseHandle(library_state_event)) {
        boundary("library-event-close", &state, state.pc);
    }
    library_state_event = NULL;
}

static unsigned reply_object(unsigned session, uint8_t *buffer, const IpcRequest *request, int kind) {
    if (request->domain) {
        for (unsigned i = 0; i < 32; i++) {
            if (!sessions[session].objects[i]) {
                sessions[session].objects[i] = kind;
                memset(&sessions[session].object_policy[i], 0, sizeof(PctlState));
                sessions[session].object_save_file[i] = NULL;
                sessions[session].object_storage[i] = 0;
                sessions[session].object_audio_output[i] = 0;
                sessions[session].object_account_pid[i] = 0;
                memset(sessions[session].object_account_user[i], 0, 16);
                memset(sessions[session].object_vibration[i], 0, sizeof(sessions[session].object_vibration[i]));
                sessions[session].object_vi_policy[i] = 0;
                sessions[session].object_vi_level[i] = 0;
                sessions[session].object_vi_displays[i] = 0;
                memset(&sessions[session].object_audio_device[i], 0, sizeof(AudioDeviceState));
                ipc_reply_domain(buffer, 256, 0, request->token, NULL, 0, i + 1);
                return i + 1;
            }
        }
        boundary("domain-object-limit", &state, state.pc);
    }
    unsigned handle = new_session(kind);
    ipc_reply_handle(buffer, 256, handle, request->token);
    return handle;
}

static void reply_storage(unsigned session, uint8_t *buffer, const IpcRequest *request, int kind, unsigned storage) {
    storage_retain(storage);
    unsigned object = reply_object(session, buffer, request, kind);
    if (request->domain) {
        sessions[session].object_storage[object - 1] = storage;
    } else {
        sessions[session_index(object)].storage = storage;
    }
}

static void reply_result(uint8_t *buffer, const IpcRequest *request, uint32_t result) {
    if (request->domain) {
        ipc_reply_domain(buffer, 256, result, request->token, NULL, 0, 0);
    } else {
        ipc_reply(buffer, 256, result, request->token);
    }
}

static void reply_value(uint8_t *buffer, const IpcRequest *request, uint32_t value) {
    if (request->domain) {
        ipc_reply_domain(buffer, 256, 0, request->token, &value, 4, 0);
    } else {
        ipc_reply_u32(buffer, 256, value, request->token);
    }
}

static void reply_wide_value(uint8_t *buffer, const IpcRequest *request, uint64_t value) {
    if (request->domain) {
        ipc_reply_domain(buffer, 256, 0, request->token, &value, 8, 0);
    } else {
        ipc_reply_u64(buffer, 256, value, request->token);
    }
}
// [extra]
