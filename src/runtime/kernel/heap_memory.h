#pragma once // [extra]
/* Horizon OS guest memory management: heap reservation, resizing, and memory query. */
#include "horizon_defs.h"

static void snapshot(void) {
    if (!snapshot_path) {
        return;
    }
    app_snapshot_frame(snapshot_path);

    FILE *input = fopen("relocation-targets.bin", "rb");
    FILE *output = fopen(snapshot_path, "wb");
    if (!input || !output) {
        fputs("Snapshot file failure\n", stderr);
        ExitProcess(33);
    }

    uint64_t address;
    unsigned count = 0;
    while (fread(&address, 8, 1, input) == 1) {
        if (++count > 1000000) {
            ExitProcess(33);
        }
        uint64_t value = LD64(address);
        if (fwrite(&value, 8, 1, output) != 1) {
            ExitProcess(33);
        }
    }
    fclose(input);
    fclose(output);

    char cpu_path[1024];
    if (snprintf(cpu_path, sizeof(cpu_path), "%s.cpu", snapshot_path) >= (int)sizeof(cpu_path)) {
        ExitProcess(33);
    }
    output = fopen(cpu_path, "wb");
    if (!output || fwrite(&state, 1, sizeof(state), output) != sizeof(state)) {
        ExitProcess(33);
    }
    fclose(output);

    /* Bounded diagnostic stack for reconstructing abort arguments. Never dump
     * arbitrary guest pointers or the multi-gigabyte heap. */
    MEMORY_BASIC_INFORMATION stack_info;
    if (STACK_SIZE <= 8 * 1024 * 1024 &&
        state.sp >= STACK_BASE &&
        state.sp <= STACK_BASE + STACK_SIZE &&
        VirtualQuery((void*)(uintptr_t)STACK_BASE, &stack_info, sizeof(stack_info)) &&
        stack_info.State == MEM_COMMIT &&
        stack_info.Protect == PAGE_READWRITE &&
        (uintptr_t)stack_info.BaseAddress <= STACK_BASE &&
        stack_info.RegionSize >= STACK_BASE - (uintptr_t)stack_info.BaseAddress + STACK_SIZE) {
        if (snprintf(cpu_path, sizeof(cpu_path), "%s.stack", snapshot_path) >= (int)sizeof(cpu_path)) {
            ExitProcess(33);
        }
        output = fopen(cpu_path, "wb");
        if (!output || fwrite((void*)(uintptr_t)STACK_BASE, 1, STACK_SIZE, output) != STACK_SIZE) {
            ExitProcess(33);
        }
        fclose(output);
    }
}

static Region query(uint64_t address) {
    uint64_t low = 0;
    uint64_t high = HORIZON_ADDRESS_SPACE_LIMIT;

    if (address >= HORIZON_ADDRESS_SPACE_LIMIT) {
        return (Region){
            .base = HORIZON_ADDRESS_SPACE_LIMIT,
            .size = 0ull - HORIZON_ADDRESS_SPACE_LIMIT,
            .kind = MEM_TYPE_RESERVED,
            .permission = PERM_NONE
        };
    }

    for (unsigned i = 0; i < HANDLE_TRANSFER_COUNT; i++) {
        if (transfer_memory[i].size &&
            address >= transfer_memory[i].address &&
            address - transfer_memory[i].address < transfer_memory[i].size) {
            return (Region){
                .base = transfer_memory[i].address,
                .size = transfer_memory[i].size,
                .kind = MEM_TYPE_HEAP,
                .permission = PERM_NONE
            };
        }
    }

    for (unsigned i = 0; i < HANDLE_SHARED_COUNT; i++) {
        if (stack_maps[i].size) {
            if (address >= stack_maps[i].source &&
                address - stack_maps[i].source < stack_maps[i].size) {
                return (Region){
                    .base = stack_maps[i].source,
                    .size = stack_maps[i].size,
                    .kind = MEM_TYPE_HEAP,
                    .permission = PERM_NONE
                };
            }
            if (address >= stack_maps[i].destination &&
                address - stack_maps[i].destination < stack_maps[i].size) {
                return (Region){
                    .base = stack_maps[i].destination,
                    .size = stack_maps[i].size,
                    .kind = MEM_TYPE_STACK,
                    .permission = PERM_RW
                };
            }
        }
    }

    size_t count = sizeof(regions) / sizeof(regions[0]);
    for (size_t i = 0; i < count + 97; i++) {
        Region r;
        if (i < count) {
            r = regions[i];
        } else if (i == count) {
            uint64_t start = HEAP_BASE;
            uint64_t end = HEAP_BASE + heap_size;

            for (unsigned j = 0; j < HANDLE_SHARED_COUNT; j++) {
                if (stack_maps[j].size) {
                    uint64_t s = stack_maps[j].source;
                    uint64_t e = s + stack_maps[j].size;
                    if (address < s && s < end) end = s;
                    if (address >= e && e > start) start = e;
                }
            }
            for (unsigned j = 0; j < HANDLE_TRANSFER_COUNT; j++) {
                if (transfer_memory[j].size) {
                    uint64_t s = transfer_memory[j].address;
                    uint64_t e = s + transfer_memory[j].size;
                    if (address < s && s < end) end = s;
                    if (address >= e && e > start) start = e;
                }
            }
            r = (Region){
                .base = start,
                .size = end - start,
                .kind = MEM_TYPE_HEAP,
                .permission = PERM_RW
            };
            if (address >= start && address < end) {
                r = attribute_clip(r, address);
            }
        } else if (i < count + 65) {
            unsigned view = i - count - 1;
            r = (Region){
                .base = shared_views[view].address,
                .size = shared_views[view].size,
                .kind = MEM_TYPE_SHARED,
                .permission = shared_views[view].permission
            };
        } else {
            unsigned slot = i - count - 65;
            r = (Region){
                .base = thread_tls[slot],
                .size = thread_tls[slot] ? 65536 : 0,
                .kind = MEM_TYPE_THREAD_LOCAL,
                .permission = PERM_RW
            };
        }

        if (!r.size) {
            continue;
        }
        if (address >= r.base && address - r.base < r.size) {
            return r;
        }
        if (address < r.base && r.base < high) {
            high = r.base;
        }
        if (address >= r.base + r.size && r.base + r.size > low) {
            low = r.base + r.size;
        }
    }

    return (Region){
        .base = low,
        .size = high - low,
        .kind = MEM_TYPE_UNMAPPED,
        .permission = PERM_NONE
    };
}

static void stop_dump(Cpu *c) {
    /* Diagnostic: AC_STOP_DUMP=x<reg>+<offset>:<bytes> dumps readable guest memory. */
    char dump[64];
    DWORD dump_n = GetEnvironmentVariableA("AC_STOP_DUMP", dump, sizeof(dump));
    unsigned dump_reg;
    unsigned long long dump_offset, dump_size;

    if (dump_n && dump_n < sizeof(dump) &&
        sscanf(dump, "x%u+%lli:%lli", &dump_reg, &dump_offset, &dump_size) == 3 &&
        dump_reg < 31 && dump_size <= 4096) {
        uint64_t at = c->x[dump_reg] + dump_offset;
        Region r = query(at);
        if ((r.permission & PERM_R) && at >= r.base &&
            at - r.base < r.size && dump_size <= r.size - (at - r.base)) {
            printf("STOP_DUMP 0x%llx", (unsigned long long)at);
            for (unsigned long long i = 0; i < dump_size; i++) {
                printf("%s%02x", i % 16 ? "" : "\n ", ((uint8_t*)(uintptr_t)at)[i]);
            }
            printf("\n");
        } else {
            printf("STOP_DUMP unavailable 0x%llx\n", (unsigned long long)at);
        }
    }
}

static uint64_t static_memory_size(void) {
    uint64_t size = 0;
    for (size_t i = 0; i < sizeof(regions) / sizeof(regions[0]); i++) {
        size += regions[i].size;
    }
    return size;
}

/* Lazy GPU write-back (Vulkan backend, exec.c): guest heap pages holding GPU results that the CPU has not read
 * yet are PAGE_NOACCESS. A CPU access faults; the vectored handler asks the backend to copy the results in and
 * lift the protection. Kernel I/O does not fault on such pages (it fails), so file reads/writes of guest buffers
 * prepare the range first. The hooks stay null with other backends. */
void (*nvn_lazy_flush_hook)(uint64_t addr, uint64_t size);
int (*nvn_lazy_fault_hook)(uint64_t addr);

#if defined(_WIN32) // [extra]
static LONG CALLBACK nvn_lazy_vectored(PEXCEPTION_POINTERS info) {
    if (info->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION ||
        info->ExceptionRecord->NumberParameters < 2 ||
        !nvn_lazy_fault_hook) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    return nvn_lazy_fault_hook((uint64_t)info->ExceptionRecord->ExceptionInformation[1])
        ? EXCEPTION_CONTINUE_EXECUTION
        : EXCEPTION_CONTINUE_SEARCH;
}

int nvn_page_protect(uint64_t addr, uint64_t size, int noaccess) {
    static int installed;
    if (!installed) {
        installed = 1;
        if (!AddVectoredExceptionHandler(1, nvn_lazy_vectored)) {
            return 0;
        }
    }
    DWORD old;
    return VirtualProtect((void*)(uintptr_t)addr, (SIZE_T)size,
                          noaccess ? PAGE_NOACCESS : PAGE_READWRITE, &old) != 0;
}

/* 0: not committed, 1: accessible, 2: committed but no access (lazy write-back protection). */
int nvn_page_state(uint64_t addr) {
    MEMORY_BASIC_INFORMATION info;
    if (!VirtualQuery((void*)(uintptr_t)addr, &info, sizeof(info)) || info.State != MEM_COMMIT) {
        return 0;
    }
    return (info.Protect & PAGE_NOACCESS) ? 2 : 1;
}
#else // [extra]
int nvn_page_protect(uint64_t addr, uint64_t size, int noaccess) {
    extra_set_lazy_fault_handler(nvn_lazy_fault_hook);
    return extra_protect_pages(addr, size, noaccess);
}
int nvn_page_state(uint64_t addr) {
    (void)addr;
    return 1;
}
#endif // [extra]

static void guest_io_prepare(uint64_t addr, uint64_t size) {
    if (nvn_lazy_flush_hook && size) {
        nvn_lazy_flush_hook(addr, size);
    }
}

static int reserve_heap_range(void) {
    if (heap_reserved) {
        return 1;
    }
    /* Choose the range before guest execution/GetInfo. Windows ASLR can occupy
     * the preferred virtual range; only probe otherwise unused host addresses. */
    for (uint64_t candidate = heap_base; candidate + HEAP_LIMIT <= HORIZON_ADDRESS_SPACE_LIMIT; candidate += HEAP_LIMIT) {
        /* Write-watched: the NVN backend re-uploads only pages the CPU wrote (plat_written_pages). */
        void *allocation = VirtualAlloc((void*)(uintptr_t)candidate, HEAP_LIMIT,
                                        MEM_RESERVE | MEM_WRITE_WATCH, PAGE_NOACCESS);
        if (allocation == (void*)(uintptr_t)candidate) {
            heap_base = candidate;
            heap_reserved = 1;
            return 1;
        }
        DWORD error = GetLastError();
        if (allocation) {
            VirtualFree(allocation, 0, MEM_RELEASE);
        }
        fprintf(stderr, "HOST heap reservation unavailable at 0x%llx (Windows error %lu)\n",
                (unsigned long long)candidate, (unsigned long)error);
    }
    return 0;
}

static uint32_t resize_heap(uint64_t size) {
    if (size % HORIZON_HEAP_ALIGNMENT || size > HEAP_LIMIT) {
        boundary("invalid-heap-size", &state, state.pc);
    }
    if (nvmap_above(HEAP_BASE + size)) {
        boundary("heap-shrink-nvmap", &state, state.pc);
    }
    for (unsigned i = 0; i < HANDLE_SHARED_COUNT; i++) {
        if (stack_maps[i].size && stack_maps[i].source + stack_maps[i].size > HEAP_BASE + size) {
            boundary("heap-shrink-borrowed-stack", &state, state.pc);
        }
    }
    for (unsigned i = 0; i < HANDLE_TRANSFER_COUNT; i++) {
        if (transfer_memory[i].size && transfer_memory[i].address + transfer_memory[i].size > HEAP_BASE + size) {
            boundary("heap-shrink-transfer-memory", &state, state.pc);
        }
    }

    uint64_t used = static_memory_size();
    if (used > MEMORY_BUDGET || size > MEMORY_BUDGET - used) {
        return RESULT_OUT_OF_MEMORY;
    }
    for (size_t i = 0; i < sizeof(regions) / sizeof(regions[0]); i++) {
        if (regions[i].base + regions[i].size > HEAP_BASE) {
            boundary("heap-layout-overlap", &state, state.pc);
        }
    }
    if (size && !reserve_heap_range()) {
        return RESULT_OUT_OF_MEMORY;
    }
    if (size > heap_size &&
        !VirtualAlloc((void*)(uintptr_t)(HEAP_BASE + heap_size), size - heap_size, MEM_COMMIT, PAGE_READWRITE)) {
        fprintf(stderr, "HOST heap commit failed (Windows error %lu)\n", (unsigned long)GetLastError());
        return RESULT_OUT_OF_MEMORY;
    }
    if (size < heap_size &&
        !VirtualFree((void*)(uintptr_t)(HEAP_BASE + size), heap_size - size, MEM_DECOMMIT)) {
        boundary("host-heap-decommit", &state, state.pc);
    }
    if (size < heap_size && update_uncached(HEAP_BASE + size, HEAP_BASE + heap_size, 0)) {
        boundary("heap-attribute-cleanup", &state, state.pc);
    }

    heap_size = size;
    InterlockedIncrement(&mapping_generation);
    return RESULT_SUCCESS;
}
// [extra]
