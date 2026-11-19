#include "memory.hpp"

namespace ac::memory {

uintptr_t get_main_base() {
    return MAIN_MODULE_BASE;
}

bool is_valid_guest_ptr(uintptr_t ptr, size_t size) {
    if (size == 0) {
        return false;
    }
    return (ptr >= GUEST_MEM_LO) && (ptr + size <= GUEST_MEM_HI);
}

} // namespace ac::memory
