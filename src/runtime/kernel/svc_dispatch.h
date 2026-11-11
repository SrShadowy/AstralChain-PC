#pragma once // [extra]
/* Horizon OS supervisor call (SVC) dispatcher and cutscene frame pacing. */
#include "horizon_defs.h"

static void rt_svc_body(Cpu *c, uint32_t imm, uint64_t pc);
static unsigned test_stall_ms;  /* (AC_CUTSCENE_STALL) slept after the kernel lock is released */

/* Parallel mode: supervisor calls run under the kernel lock; any state change may wake other guest threads; a call
 * that leaves this thread waiting blocks its host thread (lock released while asleep); an exited thread ends. */
void rt_svc(Cpu *c, uint32_t imm, uint64_t pc) {
    if (!parallel_mode) {
        rt_svc_body(c, imm, pc);
        return;
    }
    if (imm == SVC_SLEEP_THREAD) {
        /* SleepThread / yield: nothing else can end it early, so no kernel state is involved (the most frequent call:
         * worker threads poll in sleep loops). */
        int64_t ns = (int64_t)c->x[0];
        if (ns < -2) {
            boundary("thread-yield-mode", c, pc);
        }
        extra_svc_sleep_thread(ns); // [extra]
        c->x[0] = RESULT_SUCCESS;
        return;
    }

    klock();
    threads_initialize();
    klock_holder = current_thread;
    klock_holder_svc = imm;

    LARGE_INTEGER svc_a, svc_b;
    QueryPerformanceCounter(&svc_a);
    rt_svc_body(c, imm, pc);
    QueryPerformanceCounter(&svc_b);

    {
        static LARGE_INTEGER f;
        if (!f.QuadPart) {
            QueryPerformanceFrequency(&f);
        }
        double ms = 1000.0 * (double)(svc_b.QuadPart - svc_a.QuadPart) / (double)f.QuadPart;
        if (ms > SVC_LOCK_WARNING_THRESHOLD_MS) {
            printf("SVC 0x%x of guest thread %u took %.0f ms (mostly under the kernel lock)\n",
                   imm, current_thread, ms);
        }
    }

    if (guest_threads[current_thread].status != THREAD_RUNNABLE) {
        parallel_block();
    }
    if (guest_threads[current_thread].status == THREAD_EXITED) {
        printf("PARALLEL guest thread %u exited\n", current_thread);
        fflush(stdout);
        klock_release_all();
        ExitThread(0);
    }
    if (test_stall_ms) {
        static int locked = -1;
        if (locked < 0) {
            locked = GetEnvironmentVariableA("AC_CUTSCENE_STALL_LOCKED", NULL, 0) > 0;
        }
        if (locked) {
            unsigned ms = test_stall_ms;
            test_stall_ms = 0;
            Sleep(ms);
        }
    }
    kunlock();
    if (test_stall_ms) {
        unsigned ms = test_stall_ms;
        test_stall_ms = 0;
        Sleep(ms);
    }
}

/* svc #0xB1 (astral_chain/patches/cutscene-catchup-1.0.1.pchtxt): Dynamic-FPS's step code reports each frame whether
 * the game's cutscene mode is on (w21 == 0); the runtime answers in w21 (the function restores x21 on return):
 * 1 gameplay, 0 cutscene with the measured step at 60 FPS (default), 2 cutscene as in the unmodified game
 * (AC_CUTSCENE_FPS=30): the original fixed-step path at 30 FPS (nvn_present_pace, nvn_hle.h). */
int cutscene_mode;  /* (presented at 30 FPS) */
static unsigned cut_ends;
static double cut_carried;  /* (svc #0xB2, reported at a cutscene's end) */

/* Handles cutscene pacing (SVC 0xB1 injected by Dynamic-FPS patch).
 * Reports cutscene status, monitors A/V desync hitches, and compensates delta debt. */
static void handle_cutscene_pace(Cpu *c) {
    static int at30 = -1;
    static float debt;
    static int was;

    if (at30 < 0) {
        char env_val[8];
        DWORD len = GetEnvironmentVariableA("AC_CUTSCENE_FPS", env_val, sizeof(env_val));
        at30 = (len > 0 && len < sizeof(env_val) && atoi(env_val) == (int)CUTSCENE_BASE_FPS);
    }

    int cutscene = !(c->x[21] & 1);
    cutscene_mode = cutscene && at30;

    /* Monitor cutscene transitions and debug stalls */
    {
        static int last_cutscene = -1;
        static int scene_count = 0;
        static uint64_t cutscene_start_tick = 0;

        if (cutscene != last_cutscene) {
            last_cutscene = cutscene;
            if (cutscene) {
                scene_count++;
                cutscene_start_tick = rt_ticks();
            }

            printf("CUTSCENE mode %s at %.3f s\n",
                   cutscene ? "on" : "off",
                   horizon_ticks_to_seconds(rt_ticks()));

            if (!cutscene && cut_ends) {
                printf("CUTSCENE %u cut ends, %.0f ms carried into the next cuts\n",
                       cut_ends,
                       cut_carried * (double)CUTSCENE_FRAME_MS_BASE);
            }
            cut_ends = 0;
            cut_carried = 0.0;
        }

        /* AC_CUTSCENE_STALL test hook: simulates hitches at specific points */
        static int target_scene = -1;
        static double stall_at_sec;
        static unsigned stall_duration_ms;

        if (target_scene < 0) {
            char stall_env[64];
            DWORD env_len = GetEnvironmentVariableA("AC_CUTSCENE_STALL", stall_env, sizeof(stall_env));
            target_scene = 0;
            if (env_len > 0 && env_len < sizeof(stall_env)) {
                if (sscanf(stall_env, "%d:%lf:%u", &target_scene, &stall_at_sec, &stall_duration_ms) != 3) {
                    target_scene = 0;
                }
            }
        }

        if (target_scene > 0 && cutscene && scene_count == target_scene) {
            double elapsed_sec = horizon_ticks_to_seconds(rt_ticks() - cutscene_start_tick);
            if (elapsed_sec >= stall_at_sec) {
                printf("CUTSCENE test stall %u ms at %.3f s\n",
                       stall_duration_ms,
                       horizon_ticks_to_seconds(rt_ticks()));

                test_stall_ms = stall_duration_ms;

                char stall_env[64];
                DWORD env_len = GetEnvironmentVariableA("AC_CUTSCENE_STALL", stall_env, sizeof(stall_env));
                stall_env[env_len < sizeof(stall_env) ? env_len : 0] = '\0';

                static int stalls_done = 0;
                stalls_done++;

                char *ptr = stall_env;
                for (int k = 0; k < 1 + 2 * stalls_done && ptr; k++) {
                    ptr = strchr(ptr, ':');
                    if (ptr) ptr++;
                }

                target_scene = 0;
                if (ptr && sscanf(ptr, "%lf:%u", &stall_at_sec, &stall_duration_ms) == 2) {
                    target_scene = scene_count;
                }
            }
        }
    }

    /* Long frame detection: log hitches between 6 frames (~200 ms) and 9000 frames (5 min) */
    if (cutscene) {
        float measured_frames;
        uint32_t raw_bits = (uint32_t)c->v[8];
        memcpy(&measured_frames, &raw_bits, sizeof(float));

        if (measured_frames > CUTSCENE_STALL_THRESHOLD_STEPS &&
            measured_frames < CUTSCENE_MAX_SANITY_STEPS) {
            printf("CUTSCENE long frame of %.0f ms at %.3f s\n",
                   measured_frames * CUTSCENE_FRAME_MS_BASE,
                   horizon_ticks_to_seconds(rt_ticks()));
        }
    }

    if (!cutscene_mode) {
        debt = 0.0f;
        was = 0;
        return;
    }

    /* Pacing step compensation for 30 FPS fixed cutscene */
    float measured;
    uint32_t bits = (uint32_t)c->v[8];
    memcpy(&measured, &bits, sizeof(float));

    if (!was || !(measured > 0.0f) || measured > CUTSCENE_MAX_MEASURED_STEP) {
        measured = CUTSCENE_DEFAULT_STEP;
    }
    was = 1;

    debt += measured - CUTSCENE_DEFAULT_STEP;
    float step = CUTSCENE_DEFAULT_STEP;

    if (debt >= CUTSCENE_DEFAULT_STEP) {
        float extra = floorf(debt);
        if (extra > CUTSCENE_MAX_CATCHUP_PER_FRAME) {
            extra = CUTSCENE_MAX_CATCHUP_PER_FRAME;
        }
        step += extra;
        debt -= extra;
    }

    if (debt < CUTSCENE_MIN_DEBT_LIMIT) {
        debt = CUTSCENE_MIN_DEBT_LIMIT;
    }

    memcpy(&bits, &step, sizeof(float));
    c->v[8] = bits;

    /* Write tick step to animation context at offset 0xD8 */
    uint32_t ticks = (uint32_t)step;
    memcpy((void*)(uintptr_t)(c->x[19] + 0xD8), &ticks, sizeof(uint32_t));

    /* Answer in w21: mode 2 (30 FPS cutscene fixed cadence) */
    c->x[21] = (c->x[21] & ~0xFFFFFFFFull) | 2;
}

/* Handles cutscene timeline carry-over (SVC 0xB2).
 * Preserves sub-frame fractional timing across cut boundaries so audio and video stay synced. */
static void handle_cutscene_carry(Cpu *c) {
    static int carry = -1;
    if (carry < 0) {
        char env_val[8];
        DWORD len = GetEnvironmentVariableA("AC_CUT_CARRY", env_val, sizeof(env_val));
        carry = !(len > 0 && len < sizeof(env_val) && env_val[0] == '0');
    }

    static struct {
        uint64_t track;
        int32_t cut;
        float over;
    } pending[CUTSCENE_TRACK_SLOT_COUNT];

    float t, in_t, speed;
    uint32_t raw_bits = (uint32_t)c->v[0];
    memcpy(&t, &raw_bits, sizeof(float));

    raw_bits = (uint32_t)c->v[8];
    memcpy(&speed, &raw_bits, sizeof(float));

    raw_bits = (uint32_t)c->v[9];
    memcpy(&in_t, &raw_bits, sizeof(float));

    int32_t cut;
    memcpy(&cut, (void*)(uintptr_t)c->x[19], sizeof(int32_t));

    float length = (float)(int32_t)c->x[24];
    float lim = length - speed;
    uint64_t track = c->x[19];

    unsigned slot = 0;
    while (slot < CUTSCENE_TRACK_SLOT_COUNT && pending[slot].track && pending[slot].track != track) {
        slot++;
    }
    if (slot == CUTSCENE_TRACK_SLOT_COUNT) {
        slot = (unsigned)(track >> 4) % CUTSCENE_TRACK_SLOT_COUNT;
        pending[slot].track = 0;
    }

    if (!(t > lim)) {
        if (pending[slot].track == track) {
            pending[slot].track = 0;
        }
        return;
    }

    if (!carry) {
        if (in_t < lim) {
            memcpy((void*)(uintptr_t)(c->x[19] + 4), &lim, sizeof(float));
        } else {
            int32_t next = (int32_t)c->x[23];
            float zero = 0.0f;
            memcpy((void*)(uintptr_t)c->x[19], &next, sizeof(int32_t));
            memcpy((void*)(uintptr_t)(c->x[19] + 4), &zero, sizeof(float));
        }
        return;
    }

    float over = 0.0f;
    if (in_t >= lim && pending[slot].track == track && pending[slot].cut == cut) {
        over = pending[slot].over;
    }

    float real = (in_t >= lim ? lim + over : in_t) + (t - in_t);
    if (real < length) {
        memcpy((void*)(uintptr_t)(c->x[19] + 4), &lim, sizeof(float));
        pending[slot].track = track;
        pending[slot].cut = cut;
        pending[slot].over = real - lim;
    } else {
        int32_t next = (int32_t)c->x[23];
        float rest = real - length;
        memcpy((void*)(uintptr_t)c->x[19], &next, sizeof(int32_t));
        memcpy((void*)(uintptr_t)(c->x[19] + 4), &rest, sizeof(float));
        pending[slot].track = 0;
        cut_ends++;
        cut_carried += rest;
    }
}

static void rt_svc_body(Cpu *c, uint32_t imm, uint64_t pc) {
    if (imm == SVC_CUSTOM_CUTSCENE_PACE) {
        handle_cutscene_pace(c);
        return;
    }

    if (imm == SVC_CUSTOM_CUTSCENE_CARRY) {
        handle_cutscene_carry(c);
        return;
    }

    app_pump();
    if (++syscalls > SYSCALL_LIMIT) {
        boundary("svc-budget", c, pc);
    }
    HOT_LOG("SVC 0x%x\n", imm);
    if (STOP_FIRST_SVC) {
        boundary("svc-test-boundary", c, pc);
    }

    if (imm == SVC_SET_HEAP_SIZE) {
        uint32_t result = resize_heap(c->x[1]);
        c->x[0] = result;
        c->x[1] = result ? 0 : HEAP_BASE;
        return;
    }
    if (imm == SVC_CREATE_TRANSFER_MEMORY) {
        uint32_t handle = transfer_create(c->x[1], c->x[2], c->x[3]);
        c->x[0] = handle ? RESULT_SUCCESS : RESULT_OUT_OF_MEMORY;
        c->x[1] = handle;
        return;
    }
    if (imm == SVC_CLOSE_HANDLE &&
        c->x[0] >= HANDLE_TRANSFER_BASE &&
        c->x[0] < HANDLE_TRANSFER_LIMIT) {
        transfer_close((uint32_t)c->x[0]);
        c->x[0] = RESULT_SUCCESS;
        return;
    }
    if (imm == SVC_GET_SYSTEM_TICK) {
        c->x[0] = rt_ticks();
        return;
    }
    if (imm == SVC_MAP_MEMORY || imm == SVC_UNMAP_MEMORY) {
        if (imm == SVC_MAP_MEMORY) {
            stack_map(c->x[0], c->x[1], c->x[2]);
        } else {
            stack_unmap(c->x[0], c->x[1], c->x[2]);
        }
        c->x[0] = RESULT_SUCCESS;
        return;
    }
    if (imm == SVC_MAP_SHARED_MEMORY) {
        shared_map(c->x[0], c->x[1], c->x[2], c->x[3]);
        c->x[0] = RESULT_SUCCESS;
        return;
    }
    if (imm == SVC_UNMAP_SHARED_MEMORY) {
        shared_unmap(c->x[0], c->x[1], c->x[2]);
        c->x[0] = RESULT_SUCCESS;
        return;
    }
    if (imm == SVC_CLOSE_HANDLE &&
        c->x[0] >= HANDLE_SHARED_BASE &&
        c->x[0] < HANDLE_SHARED_LIMIT &&
        shared_handles[c->x[0] - HANDLE_SHARED_BASE]) {
        unsigned slot = c->x[0] - HANDLE_SHARED_BASE;
        if (!CloseHandle(shared_handles[slot])) {
            boundary("host-shared-handle-close", c, pc);
        }
        shared_handles[slot] = NULL;
        shared_sources[slot] = NULL;
        shared_permissions[slot] = 0;
        shared_sizes[slot] = 0;
        c->x[0] = RESULT_SUCCESS;
        return;
    }
    if ((imm == SVC_CLOSE_HANDLE || imm == SVC_RESET_SIGNAL) &&
        c->x[0] >= HANDLE_EVENT_BASE &&
        c->x[0] < HANDLE_EVENT_LIMIT &&
        events[c->x[0] - HANDLE_EVENT_BASE].handle) {
        unsigned event = c->x[0] - HANDLE_EVENT_BASE;
        if (imm == SVC_CLOSE_HANDLE) {
            if (!CloseHandle(events[event].handle)) {
                boundary("host-event-close", c, pc);
            }
            events[event].handle = NULL;
            events[event].access = 0;
        } else {
            if (!ResetEvent(events[event].handle)) {
                boundary("host-event-reset", c, pc);
            }
        }
        c->x[0] = RESULT_SUCCESS;
        return;
    }

    if (imm == SVC_WAIT_SYNCHRONIZATION) {
        uint64_t count = c->x[2], address = c->x[1];
        Region r = query(address);
        if (count < 1 || count > 64 || !(r.permission & PERM_R) ||
            count * 4 > r.size - (address - r.base)) {
            boundary("wait-handle-list", c, pc);
        }
        uint32_t list[64];
        unsigned threads_waited = 0;
        for (unsigned i = 0; i < count; i++) {
            list[i] = LD32(address + 4 * i);
            uint32_t h = list[i];
            if (h == HANDLE_MAIN_THREAD || (h >= HANDLE_THREAD_BASE && h < HANDLE_THREAD_LIMIT)) {
                thread_index(h);
                threads_waited++;
            } else if (h < HANDLE_EVENT_BASE || h >= HANDLE_EVENT_LIMIT || !events[h - HANDLE_EVENT_BASE].handle) {
                printf("WAIT handle=0x%x index=%u count=%llu\n", h, i, (unsigned long long)count);
                boundary("wait-object-unimplemented", c, pc);
            }
        }
        threads_initialize();
        int ready = thread_objects_ready(list, (unsigned)count);
        if (ready >= 0) {
            c->x[0] = RESULT_SUCCESS;
            c->x[1] = (uint64_t)ready;
            return;
        }
        int64_t ns = (int64_t)c->x[3];
        if (ns == 0) {
            c->x[0] = RESULT_TIMEOUT;
            c->x[1] = 0xffffffff;
            return;
        }
        if (ns < -1) {
            c->x[0] = RESULT_INTERRUPTED;
            return;
        }
        unsigned others = 0;
        for (unsigned i = 0; i < 33; i++) {
            if (i != current_thread &&
                guest_threads[i].status != THREAD_UNUSED &&
                guest_threads[i].status != THREAD_CREATED &&
                guest_threads[i].status != THREAD_EXITED) {
                others++;
            }
        }
        if (others) {
            /* Cooperative wait: park this context; thread_checkpoint polls the list and
             * writes the result (x0, x1) when a handle signals or the deadline passes. */
            guest_threads[current_thread].status = THREAD_WAIT_OBJECTS;
            guest_threads[current_thread].wait_count = (unsigned)count;
            memcpy(guest_threads[current_thread].wait_handles, list, count * 4);
            guest_threads[current_thread].wait_deadline = (ns == -1) ? 0 : thread_deadline(ns);
            (parallel_mode ? 0 : (thread_reschedule = 1));
            return;
        }
        if (threads_waited) {
            boundary("thread-wait-without-other-context", c, pc);
        }
        HANDLE handles[64];
        for (unsigned i = 0; i < count; i++) {
            handles[i] = events[list[i] - HANDLE_EVENT_BASE].handle;
        }
        DWORD timeout;
        if (ns == -1) {
            timeout = app_window ? INFINITE : 0;
        } else {
            uint64_t ms = ((uint64_t)ns + 999999) / 1000000;
            timeout = (ms >= INFINITE) ? INFINITE - 1 : (DWORD)ms;
        }
        DWORD result = app_wait((DWORD)count, handles, timeout);
        if (result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + count) {
            c->x[0] = RESULT_SUCCESS;
            c->x[1] = result - WAIT_OBJECT_0;
            return;
        }
        if (result == WAIT_TIMEOUT) {
            if (ns == -1) {
                boundary("event-wait-needs-producer", c, pc);
            }
            c->x[0] = RESULT_TIMEOUT;
            c->x[1] = 0xffffffff;
            return;
        }
        boundary("host-event-wait", c, pc);
    }

    if (ipc_svc(c, imm, pc)) {
        return;
    }

    if (imm == SVC_SET_MEMORY_ATTRIBUTE) {
        uint64_t address = c->x[0], size = c->x[1];
        unsigned mask = (uint32_t)c->x[2], value = (uint32_t)c->x[3];
        if (address & HORIZON_PAGE_MASK) {
            c->x[0] = RESULT_INVALID_ADDRESS;
            return;
        }
        if (!size || (size & HORIZON_PAGE_MASK)) {
            c->x[0] = RESULT_INVALID_SIZE;
            return;
        }
        if ((mask & ~8u) || (value & ~mask)) {
            boundary("memory-attribute-mask", c, pc);
        }
        if (address < HEAP_BASE || address >= HEAP_BASE + heap_size ||
            size > HEAP_BASE + heap_size - address) {
            c->x[0] = RESULT_INVALID_CURRENT_MEMORY;
            return;
        }
        uint64_t end = address + size;
        for (uint64_t at = address; at < end;) {
            Region r = query(at);
            if (r.kind != MEM_TYPE_HEAP || r.permission != PERM_RW) {
                c->x[0] = RESULT_INVALID_CURRENT_MEMORY;
                return;
            }
            at = r.base + r.size;
        }
        c->x[0] = mask ? update_uncached(address, end, value != 0) : RESULT_SUCCESS;
        return;
    }

    if (imm == SVC_QUERY_MEMORY) {
        Region destination = query(c->x[0]);
        if (!(destination.permission & PERM_W) ||
            c->x[0] - destination.base > destination.size ||
            destination.size - (c->x[0] - destination.base) < 40) {
            boundary("query-output-outside-writable-memory", c, pc);
        }
        Region result = query(c->x[2]);
        uint64_t info[5] = {result.base, result.size, result.kind, result.permission, 0};
        info[2] |= (uint64_t)memory_attribute(c->x[2]) << 32;
        for (unsigned i = 0; i < HANDLE_SHARED_COUNT; i++) {
            if (stack_maps[i].size &&
                c->x[2] >= stack_maps[i].source &&
                c->x[2] - stack_maps[i].source < stack_maps[i].size) {
                info[2] |= 1ull << 32; /* MemoryAttribute::IsBorrowed */
            }
        }
        for (unsigned i = 0; i < HANDLE_TRANSFER_COUNT; i++) {
            if (transfer_memory[i].size &&
                c->x[2] >= transfer_memory[i].address &&
                c->x[2] - transfer_memory[i].address < transfer_memory[i].size) {
                info[2] |= 1ull << 32;
            }
        }
        memcpy((void*)(uintptr_t)c->x[0], info, 40);
        c->x[0] = RESULT_SUCCESS;
        c->x[1] = 0;
        return;
    }

    if (imm == SVC_GET_INFO && c->x[1] == INFO_RANDOM_ENTROPY && c->x[2] == 0 && c->x[3] < 4) {
        c->x[1] = entropy[c->x[3]];
        c->x[0] = RESULT_SUCCESS;
        return;
    }

    if (thread_svc(c, imm)) {
        return;
    }

    if (imm == SVC_GET_INFO && c->x[2] == HANDLE_CURRENT_PROCESS && c->x[3] == 0) {
        uint64_t result;
        switch (c->x[1]) {
            case INFO_CORE_MASK:
                result = PROCESS_CORE_MASK;
                break;
            case INFO_PRIORITY_MASK:
                result = PROCESS_PRIORITY_MASK;
                break;
            case INFO_ALIAS_REGION_ADDRESS:
                result = HORIZON_ALIAS_REGION_ADDRESS;
                break;
            case INFO_ALIAS_REGION_SIZE:
                result = HORIZON_ALIAS_REGION_SIZE;
                break;
            case INFO_HEAP_REGION_ADDRESS:
                result = HEAP_BASE;
                break;
            case INFO_HEAP_REGION_SIZE:
                result = HEAP_LIMIT;
                break;
            case INFO_TOTAL_MEMORY_SIZE:
            case INFO_INITIAL_PROCESS_ID_RANGE:
                result = MEMORY_BUDGET;
                break;
            case INFO_USED_MEMORY_SIZE:
            case INFO_USER_EXCEPTION_CONTEXT_ADDRESS:
                result = static_memory_size() + heap_size;
                break;
            case INFO_ASLR_REGION_ADDRESS:
                result = 0;
                break;
            case INFO_ASLR_REGION_SIZE:
                result = HORIZON_ADDRESS_SPACE_LIMIT;
                break;
            case INFO_STACK_REGION_ADDRESS:
                result = STACK_BASE;
                break;
            case INFO_STACK_REGION_SIZE:
                result = HORIZON_STACK_REGION_SIZE;
                break;
            case INFO_SYSTEM_RESOURCE_SIZE:
                result = SYSTEM_RESOURCE_SIZE;
                break;
            case INFO_SYSTEM_RESOURCE_USED:
                if (SYSTEM_RESOURCE_SIZE) {
                    boundary("system-resource-accounting", c, pc);
                }
                result = 0;
                break;
            default:
                boundary("get-info", c, pc);
        }
        c->x[1] = result;
        c->x[0] = RESULT_SUCCESS;
        return;
    }

    boundary("svc", c, pc);
}
// [extra]
