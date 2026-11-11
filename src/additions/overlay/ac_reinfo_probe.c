/* [extra] C side of the RE Info window: exposes the main-module load address and
 * the guest address window to ac_reinfo.cpp, so the C++ translation unit does
 * not have to include the runtime headers (cpu.h / rt.h) next to ImGui.
 *
 * The Linux build links no loader.c: native_trace emits one fixed
 * reserve(module.base, ...) per module, and the linker places them in order
 * rtld, main, sdk from 0x7000000000. rtld is 0x4000 bytes, so main lands at
 * 0x7000010000 - the same base the RE documents and re_cursor.py use for
 * "main+0x...". Re-derive if the module order or rtld's size ever changes. */
#define AC_MAIN_BASE 0x7000010000ull

#include "cpu/rt.h"

unsigned long long ac_reinfo_main_base(void) { return AC_MAIN_BASE; }
unsigned long long ac_reinfo_guest_lo(void) { return GUEST_LO; }
unsigned long long ac_reinfo_guest_hi(void) { return GUEST_HI; }

#include <string.h>

int extra_get_guest_voice_language(void) {
    unsigned long long base = ac_reinfo_main_base();
    if (!base) return -1;

    /* 1. If sound engine is active and has resolved a voice bank, read active language (*0xdc2908 -> 0xdb5cb0). */
    unsigned long long ptr_se = 0;
    uintptr_t slot_se = (uintptr_t)(base + 0xdc2908);
    memcpy(&ptr_se, (const void*)slot_se, sizeof ptr_se);
    if (ptr_se >= GUEST_LO && ptr_se + 4 <= GUEST_HI) {
        int32_t val = -1;
        memcpy(&val, (const void*)(uintptr_t)ptr_se, sizeof val);
        if (val == 0 || val == 1) return (int)val;
    }

    /* 2. Otherwise read OptionManager voice language (*0xdc13b0 -> 0xdafb14). */
    unsigned long long ptr_opt = 0;
    uintptr_t slot_opt = (uintptr_t)(base + 0xdc13b0);
    memcpy(&ptr_opt, (const void*)slot_opt, sizeof ptr_opt);
    if (ptr_opt >= GUEST_LO && ptr_opt + 4 <= GUEST_HI) {
        int32_t val = -1;
        memcpy(&val, (const void*)(uintptr_t)ptr_opt, sizeof val);
        if (val == 0 || val == 1) return (int)val;
    }
    return -1;
}

void extra_set_guest_voice_language(int voice_lang) {
    if (voice_lang < -1 || voice_lang > 1) return;
    unsigned long long base = ac_reinfo_main_base();
    if (!base) return;

    /* 1. Write the voice language value to OptionManager (*0xdc13b0 -> 0xdafb14).
     * 0 = Japanese ("Japanese"), 1 = English ("English(US)"), -1 = Auto. */
    unsigned long long ptr_opt = 0;
    uintptr_t slot_opt = (uintptr_t)(base + 0xdc13b0);
    memcpy(&ptr_opt, (const void*)slot_opt, sizeof ptr_opt);
    if (ptr_opt >= GUEST_LO && ptr_opt + 4 <= GUEST_HI) {
        int32_t val = (int32_t)voice_lang;
        memcpy((void*)(uintptr_t)ptr_opt, &val, sizeof val);
    }

    /* 2. Raise dirty flag 1 at *0xdc13b8 (points to 0xcca5974). */
    unsigned long long ptr_d1 = 0;
    uintptr_t slot_d1 = (uintptr_t)(base + 0xdc13b8);
    memcpy(&ptr_d1, (const void*)slot_d1, sizeof ptr_d1);
    if (ptr_d1 >= GUEST_LO && ptr_d1 + 4 <= GUEST_HI) {
        uint32_t one = 1;
        memcpy((void*)(uintptr_t)ptr_d1, &one, sizeof one);
    }

    /* 3. Raise dirty flag 2 at *0xdc13c0 (points to 0xcca5978) to trigger Wwise soundbank reload. */
    unsigned long long ptr_d2 = 0;
    uintptr_t slot_d2 = (uintptr_t)(base + 0xdc13c0);
    memcpy(&ptr_d2, (const void*)slot_d2, sizeof ptr_d2);
    if (ptr_d2 >= GUEST_LO && ptr_d2 + 4 <= GUEST_HI) {
        uint32_t one = 1;
        memcpy((void*)(uintptr_t)ptr_d2, &one, sizeof one);
    }
}
