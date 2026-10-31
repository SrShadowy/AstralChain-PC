// [extra]
#include "../../exec_internal.h"

#define VCOPY_TAB 32768
static struct { uint64_t addr, size, seq; VkSpan span; } vcopy[VCOPY_TAB];
static uint64_t vcopy_gen;

void guest_vcopy_flush(void) {
    vcopy_gen++;
}

VkSpan guest_copy_vtx(uint64_t addr, uint64_t size) {
    uint64_t seq = vk_pending_seq() + (vcopy_gen << 40);
    uint64_t h = (addr * 0x9E3779B97F4A7C15ull) ^ (size * 0xC2B2AE3D27D4EB4Full); // [extra]
    uint32_t slot = (uint32_t)(h ^ (h >> 32)) & (VCOPY_TAB - 1);
    if (vcopy[slot].seq == seq && vcopy[slot].addr == addr && vcopy[slot].size == size) return vcopy[slot].span; // [extra]
    stat_up_vtx += size;
    VkSpan s = vk_transient_copy((const void *)(uintptr_t)addr, size, 256);
    vcopy[slot].addr = addr; vcopy[slot].size = size; vcopy[slot].seq = seq; vcopy[slot].span = s;
    return s;
}
// [extra]
