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
