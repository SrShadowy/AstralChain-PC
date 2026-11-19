#pragma once
#include <cstdint>
#include "memory.hpp"

namespace ac::time {

constexpr uintptr_t TIMER_MANAGER_PTR_SLOT      = 0xdb89a0ull;
constexpr uintptr_t TIMER_MANAGER_DIRECT_OFFSET = 0xdcbfd0ull;
constexpr uintptr_t SETUP_TIMER_FN              = 0x3e3cf4ull;
constexpr uintptr_t UPDATE_TIMER_FN             = 0x91dbd0ull;

constexpr uintptr_t OFFSET_TIME_SCALE_GAME      = 0xb0ull;
constexpr uintptr_t OFFSET_FRAME_DELTA_NORM     = 0xd0ull;
constexpr uintptr_t OFFSET_TARGET_RATE          = 0xdcull;
constexpr uintptr_t OFFSET_FRAME_DELTA_MS       = 0xe0ull;

float get_frame_delta_ms();
float get_target_fps();
float get_game_time_scale();
void set_game_time_scale(float scale);

} // namespace ac::time
