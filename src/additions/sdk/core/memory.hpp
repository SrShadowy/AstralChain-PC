#pragma once
#include <cstdint>
#include <cstring>
#include "types.hpp"

namespace ac::memory {

constexpr uintptr_t MAIN_MODULE_BASE = 0x7000010000ull;
constexpr uintptr_t GUEST_MEM_LO     = 0x7000000000ull;
constexpr uintptr_t GUEST_MEM_HI     = 0x7320000000ull; // 12.5 GB guest window (matches GUEST_HI in rt.h)

// Non-template runtime queries implemented in memory.cpp
uintptr_t get_main_base();
bool is_valid_guest_ptr(uintptr_t ptr, size_t size);
bool is_guest_memory_accessible();

template <typename T>
inline T* resolve(uintptr_t module_offset) {
    if (!is_guest_memory_accessible()) {
        return nullptr;
    }
    uintptr_t addr = (module_offset >= GUEST_MEM_LO) ? module_offset : (MAIN_MODULE_BASE + module_offset);
    if (!is_valid_guest_ptr(addr, sizeof(T))) {
        return nullptr;
    }
    return reinterpret_cast<T*>(addr);
}

template <typename T>
inline bool read_raw(uintptr_t addr, T* out_value) {
    if (!out_value || !is_guest_memory_accessible()) {
        return false;
    }
    if (!is_valid_guest_ptr(addr, sizeof(T))) {
        return false;
    }
    const void* src = reinterpret_cast<const void*>(addr);
    std::memcpy(out_value, src, sizeof(T));
    return true;
}

template <typename T>
inline bool read(uintptr_t module_offset, T* out_value) {
    if (!out_value || !is_guest_memory_accessible()) {
        return false;
    }
    uintptr_t addr = (module_offset >= GUEST_MEM_LO) ? module_offset : (MAIN_MODULE_BASE + module_offset);
    return read_raw<T>(addr, out_value);
}

template <typename T>
inline bool write(uintptr_t module_offset, const T& in_value) {
    if (!is_guest_memory_accessible()) {
        return false;
    }
    uintptr_t addr = (module_offset >= GUEST_MEM_LO) ? module_offset : (MAIN_MODULE_BASE + module_offset);
    if (!is_valid_guest_ptr(addr, sizeof(T))) {
        return false;
    }
    void* dst = reinterpret_cast<void*>(addr);
    std::memcpy(dst, &in_value, sizeof(T));
    return true;
}

template <typename T>
inline bool read_deref(uintptr_t pointer_slot_offset, uintptr_t struct_offset, T* out_value) {
    if (!out_value || !is_guest_memory_accessible()) {
        return false;
    }
    uintptr_t base_ptr = 0;
    if (!read<uintptr_t>(pointer_slot_offset, &base_ptr) || !base_ptr || (base_ptr & 3) != 0) {
        return false;
    }
    return read_raw<T>(base_ptr + struct_offset, out_value);
}

template <typename T>
inline bool write_deref(uintptr_t pointer_slot_offset, uintptr_t struct_offset, const T& in_value) {
    if (!is_guest_memory_accessible()) {
        return false;
    }
    uintptr_t base_ptr = 0;
    if (!read<uintptr_t>(pointer_slot_offset, &base_ptr) || !base_ptr || (base_ptr & 3) != 0) {
        return false;
    }
    uintptr_t target_addr = base_ptr + struct_offset;
    if (!is_valid_guest_ptr(target_addr, sizeof(T))) {
        return false;
    }
    void* dst = reinterpret_cast<void*>(target_addr);
    std::memcpy(dst, &in_value, sizeof(T));
    return true;
}

// Canonical AOB Signatures for dynamic function & slot resolution
constexpr const char* AOB_GET_CAMERA_MANAGER =
    "?? ?? ?? b0 00 d0 41 f9 c0 03 5f d6 00 00 00 00 ?? ?? ?? b0 08 d1 41 f9";
constexpr const char* AOB_TIMER_MANAGER =
    "?? ?? ?? 90 08 d1 44 f9 00 d1 40 bd ?? ?? ?? 90 01 8d 4e bd 00 08 21 1e";
constexpr const char* AOB_ENGINE_STATE =
    "?? ?? ?? b0 4a cd 41 f9 4a 31 40 b9 0b f4 9d 52 4b 01 a0 72 4a 09 13 12 29 69 6b b8 49 01 09 2a ?? ?? ?? 35";
constexpr const char* AOB_SAVE =
    "?? ?? ?? 90 08 ed 41 f9 09 c0 9d 52 09 01 a0 72 01 69 69 b8 89 cc 9d 52 09 01 a0 72 14 69 69 b8";
constexpr const char* AOB_SCENE =
    "?? ?? ?? d0 29 65 42 f9 6a 32 48 b9 29 81 41 b9 29 3d 10 53 5f 05 00 31";
constexpr const char* AOB_PHASE =
    "?? ?? ?? 90 21 7c 42 f9 e0 43 0b 91 ?? ?? ?? 94 e0 43 0b 91 ?? ?? ?? 94";
constexpr const char* AOB_SCENARIO =
    "?? ?? ?? 90 08 99 44 f9 09 f5 52 b9 e0 03 1f aa ?? ?? ?? 37 0a 29 4c b9";
constexpr const char* AOB_PLAYER_TABLE =
    "?? ?? ?? f0 08 41 43 f9 00 59 60 f8 c0 03 5f d6 ?? ?? ?? 90 08 ed 41 f9";
constexpr const char* AOB_MISSION =
    "?? ?? ?? d0 08 11 42 f9 00 01 40 b9 c0 03 5f d6 f4 4f be a9 fd 7b 01 a9";
constexpr const char* AOB_BOSS_ACTIVE =
    "?? ?? ?? 90 08 95 44 f9 08 01 40 b9 1f 05 00 71 ?? ?? ?? 54 ?? ?? ?? 14";
constexpr const char* AOB_AREA =
    "?? ?? ?? 90 ?? ?? ?? 90 29 89 47 f9 4a 8d 47 f9 0b 00 80 12 3f 01 00 b9";
constexpr const char* AOB_SUBAREA =
    "?? ?? ?? 90 08 91 47 f9 00 01 40 b9 c0 03 5f d6 ff c3 01 d1 f5 23 00 f9";
constexpr const char* AOB_MENU_PTR =
    "fd 7b bf a9 fd 03 00 91 ?? ?? ?? 94 ?? ?? ?? f0 08 41 46 f9 09 00 80 12";
constexpr const char* AOB_MENU_SELECTION =
    "ff c3 07 d1 fc f3 00 f9 0a 90 41 b9 e8 03 00 91 ?? ?? ?? 34 ?? ?? ?? 90";
constexpr const char* AOB_VOICE_OPT =
    "?? ?? ?? 90 08 d9 41 f9 09 01 40 b9 3f 01 00 6b ?? ?? ?? 54 ?? ?? ?? 90";
constexpr const char* AOB_PAD_STATE_TABLE =
    "?? ?? ?? d0 29 8d 42 f9 0a 14 80 52 08 25 0a 9b e9 03 60 b2 00 e1 00 91";
constexpr const char* AOB_PAD_INPUT_STATE =
    "22 01 22 1e ?? ?? ?? d0 63 01 22 1e 29 a1 42 f9 80 01 22 1e e4 03 27 1e";
constexpr const char* AOB_VOICE_SE =
    "?? ?? ?? f0 08 85 44 f9 1f 2c 00 71 00 01 00 b9 ?? ?? ?? 54 ?? ?? ?? d0 08 81 36 91 00 d9 60 f8 fd 7b c1 a8";
constexpr const char* AOB_OPEN_MENU =
    "f3 0f 1e f8 fd 7b 01 a9 fd 43 00 91 3f 30 00 71 ?? ?? ?? 54 e8 03 01 2a";
constexpr const char* AOB_CAMERA_DIRECTOR_UPDATE =
    "f3 0f 1e f8 fd 7b 01 a9 fd 43 00 91 f3 03 00 aa 68 46 46 b9 1f b5 03 71";

uintptr_t scan_aob(const char* pattern, uintptr_t search_start = 0, size_t search_len = 0);
uintptr_t decode_adrp_ldr(uintptr_t code_addr, size_t max_ins_scan = 12);
uintptr_t resolve_slot_from_aob(const char* pattern, uintptr_t fallback_offset, size_t max_ins_scan = 12);

} // namespace ac::memory
