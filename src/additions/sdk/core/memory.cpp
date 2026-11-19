#include "memory.hpp"
#include <cstdlib>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

namespace ac::memory {

uintptr_t get_main_base() {
    return MAIN_MODULE_BASE;
}

bool is_guest_memory_accessible() {
    static int s_accessible = -1;
    if (s_accessible == -1) {
        unsigned char vec = 0;
        uintptr_t page_addr = MAIN_MODULE_BASE & ~0xFFFull;
        int res = mincore(reinterpret_cast<void*>(page_addr), 4096, &vec);
        s_accessible = (res == 0) ? 1 : 0;
    }
    return s_accessible == 1;
}

bool is_valid_guest_ptr(uintptr_t ptr, size_t size) {
    if (size == 0) {
        return false;
    }
    return (ptr >= GUEST_MEM_LO) && (ptr + size >= ptr) && (ptr + size <= GUEST_MEM_HI);
}

uintptr_t scan_aob(const char* pattern, uintptr_t search_start, size_t search_len) {
    if (!pattern || !is_guest_memory_accessible()) {
        return 0;
    }

    struct SigByte {
        uint8_t value;
        bool wildcard;
    };

    std::vector<SigByte> sig;
    const char* p = pattern;
    while (*p) {
        while (*p == ' ') {
            p++;
        }
        if (!*p) {
            break;
        }
        if (*p == '?') {
            sig.push_back({0, true});
            while (*p == '?') {
                p++;
            }
        } else {
            char* end = nullptr;
            unsigned long val = std::strtoul(p, &end, 16);
            if (end == p) {
                break;
            }
            sig.push_back({static_cast<uint8_t>(val), false});
            p = end;
        }
    }

    if (sig.empty()) {
        return 0;
    }

    uintptr_t start = search_start ? search_start : MAIN_MODULE_BASE;
    size_t len = search_len ? search_len : 0xa0e7d0ull; // default .text size
    if (!is_valid_guest_ptr(start, len)) {
        return 0;
    }

    const uint8_t* mem = reinterpret_cast<const uint8_t*>(start);
    size_t sig_len = sig.size();
    if (len < sig_len) {
        return 0;
    }

    for (size_t i = 0; i <= len - sig_len; i += 4) {
        bool match = true;
        for (size_t j = 0; j < sig_len; ++j) {
            if (!sig[j].wildcard && mem[i + j] != sig[j].value) {
                match = false;
                break;
            }
        }
        if (match) {
            return start + i;
        }
    }
    return 0;
}

uintptr_t decode_adrp_ldr(uintptr_t code_addr, size_t max_ins_scan) {
    if (!code_addr || (code_addr & 3) != 0) {
        return 0;
    }
    const uint32_t* ins_ptr = reinterpret_cast<const uint32_t*>(code_addr);
    for (size_t i = 0; i < max_ins_scan; ++i) {
        uint32_t w = ins_ptr[i];
        // ADRP: (w & 0x9f000000) == 0x90000000
        if ((w & 0x9f000000u) == 0x90000000u) {
            uintptr_t curr_pc = code_addr + i * sizeof(uint32_t);
            uint32_t immlo = (w >> 29) & 0x3u;
            uint32_t immhi = (w >> 5) & 0x7ffffu;
            int32_t imm = static_cast<int32_t>((immhi << 2) | immlo);
            if (imm & (1 << 20)) {
                imm -= (1 << 21);
            }
            uintptr_t page = (curr_pc & ~0xfffull) + (static_cast<int64_t>(imm) << 12);
            uint32_t rd = w & 0x1fu;

            for (size_t j = 1; j < 8 && (i + j) < (max_ins_scan + 4); ++j) {
                uint32_t w2 = ins_ptr[i + j];
                // LDR 64-bit: 0xf9400000
                if ((w2 & 0xffc00000u) == 0xf9400000u && ((w2 >> 5) & 0x1fu) == rd) {
                    uint32_t imm12 = (w2 >> 10) & 0xfffu;
                    uintptr_t abs_target = page + (static_cast<uintptr_t>(imm12) << 3);
                    return (abs_target >= MAIN_MODULE_BASE) ? (abs_target - MAIN_MODULE_BASE) : abs_target;
                }
                // ADD 64-bit: 0x91000000
                if ((w2 & 0xff800000u) == 0x91000000u && ((w2 >> 5) & 0x1fu) == rd) {
                    uint32_t imm12 = (w2 >> 10) & 0xfffu;
                    uint32_t sh = (w2 >> 22) & 1u;
                    uintptr_t abs_target = page + (static_cast<uintptr_t>(imm12) << (sh ? 12 : 0));
                    return (abs_target >= MAIN_MODULE_BASE) ? (abs_target - MAIN_MODULE_BASE) : abs_target;
                }
                // LDR 32-bit: 0xb9400000
                if ((w2 & 0xffc00000u) == 0xb9400000u && ((w2 >> 5) & 0x1fu) == rd) {
                    uint32_t imm12 = (w2 >> 10) & 0xfffu;
                    uintptr_t abs_target = page + (static_cast<uintptr_t>(imm12) << 2);
                    return (abs_target >= MAIN_MODULE_BASE) ? (abs_target - MAIN_MODULE_BASE) : abs_target;
                }
            }
        }
    }
    return 0;
}

uintptr_t resolve_slot_from_aob(const char* pattern, uintptr_t fallback_offset, size_t max_ins_scan) {
    if (!is_guest_memory_accessible() || !pattern) {
        return fallback_offset;
    }
    uintptr_t match = scan_aob(pattern);
    if (!match) {
        return fallback_offset;
    }
    uintptr_t slot = decode_adrp_ldr(match, max_ins_scan);
    return slot ? slot : fallback_offset;
}

} // namespace ac::memory
