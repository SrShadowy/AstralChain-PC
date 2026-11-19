#pragma once
#include <cstdint>
#include "../core/types.hpp"
#include "../core/memory.hpp"

namespace ac::input {

constexpr uintptr_t PAD_INPUT_STATE_OFFSET = 0xdc2540ull;
constexpr uintptr_t PAD_STATE_TABLE_OFFSET = 0xdc2518ull;
constexpr uintptr_t PAD_SLOT_SIZE          = 0xa0ull;
constexpr uintptr_t GET_STICK_VECTOR_FN    = 0x9ad0a0ull;

constexpr uintptr_t OFFSET_STICK_R_X       = 0x0cull;
constexpr uintptr_t OFFSET_STICK_R_Y       = 0x10ull;
constexpr uintptr_t OFFSET_STICK_L_Y       = 0x14ull;
constexpr uintptr_t OFFSET_STICK_L_X       = 0x18ull;
constexpr uintptr_t OFFSET_BUTTONS_HELD    = 0x20ull;

StickVector get_right_stick();
StickVector get_left_stick();
uint32_t get_buttons_held();

} // namespace ac::input
