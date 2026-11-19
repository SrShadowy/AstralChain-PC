#pragma once
#include <cstdint>
#include <cstring>
#include "types.hpp"

namespace ac::memory {

constexpr uintptr_t MAIN_MODULE_BASE = 0x7000010000ull;
constexpr uintptr_t GUEST_MEM_LO     = 0x7000000000ull;
constexpr uintptr_t GUEST_MEM_HI     = 0x8000000000ull;

// Non-template runtime queries implemented in memory.cpp
uintptr_t get_main_base();
bool is_valid_guest_ptr(uintptr_t ptr, size_t size);

template <typename T>
inline T* resolve(uintptr_t module_offset) {
    uintptr_t addr = MAIN_MODULE_BASE + module_offset;
    return reinterpret_cast<T*>(addr);
}

template <typename T>
inline bool read(uintptr_t module_offset, T* out_value) {
    if (!out_value) {
        return false;
    }
    const void* src = reinterpret_cast<const void*>(MAIN_MODULE_BASE + module_offset);
    std::memcpy(out_value, src, sizeof(T));
    return true;
}

template <typename T>
inline bool write(uintptr_t module_offset, const T& in_value) {
    void* dst = reinterpret_cast<void*>(MAIN_MODULE_BASE + module_offset);
    std::memcpy(dst, &in_value, sizeof(T));
    return true;
}

template <typename T>
inline bool read_deref(uintptr_t pointer_slot_offset, uintptr_t struct_offset, T* out_value) {
    if (!out_value) {
        return false;
    }
    uintptr_t base_ptr = 0;
    if (!read<uintptr_t>(pointer_slot_offset, &base_ptr) || !base_ptr) {
        return false;
    }
    const void* src = reinterpret_cast<const void*>(base_ptr + struct_offset);
    std::memcpy(out_value, src, sizeof(T));
    return true;
}

template <typename T>
inline bool write_deref(uintptr_t pointer_slot_offset, uintptr_t struct_offset, const T& in_value) {
    uintptr_t base_ptr = 0;
    if (!read<uintptr_t>(pointer_slot_offset, &base_ptr) || !base_ptr) {
        return false;
    }
    void* dst = reinterpret_cast<void*>(base_ptr + struct_offset);
    std::memcpy(dst, &in_value, sizeof(T));
    return true;
}

} // namespace ac::memory
