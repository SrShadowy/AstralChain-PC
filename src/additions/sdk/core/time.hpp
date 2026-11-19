#pragma once
#include <cstdint>
#include "memory.hpp"

namespace ac::time {

constexpr uintptr_t TIMER_MANAGER_PTR_SLOT      = 0xdb89a0ull;
constexpr uintptr_t TIMER_MANAGER_DIRECT_OFFSET = 0xdcbfd0ull;
constexpr uintptr_t SETUP_TIMER_FN              = 0x3e3cf4ull;
constexpr uintptr_t UPDATE_TIMER_FN             = 0x91dbd0ull;

// TimerManager Internal Offsets
constexpr uintptr_t OFFSET_TIME_SCALE_GAME      = 0xb0ull; // float: Game speed scale (1.0f)
constexpr uintptr_t OFFSET_TIME_SCALE_SLOW      = 0xb8ull; // float: Slow motion scale / IRIS (0.2f)
constexpr uintptr_t OFFSET_FRAME_STEP           = 0xd0ull; // float: Normalized simulation step
constexpr uintptr_t OFFSET_REMAINDER_TICKS      = 0xd4ull; // float: Fractional tick remainder
constexpr uintptr_t OFFSET_WHOLE_TICKS          = 0xd8ull; // uint32: Whole 30 FPS ticks elapsed
constexpr uintptr_t OFFSET_TARGET_RATE          = 0xdcull; // float: Frames per millisecond (fps / 1000.0f)
constexpr uintptr_t OFFSET_FRAME_DELTA_MS       = 0xe0ull; // float: Real measured frame time in ms

// Pacing & Simulation Step Bounds
constexpr float BASE_SIMULATION_FPS             = 30.0f;
constexpr float MIN_FRAME_STEP_FLOOR            = 1.0f / 16.0f; // 0.0625f (up to 480 FPS)
constexpr float MAX_FRAME_STEP_GAMEPLAY         = 2.0f;         // 2.0 ticks (at 15 FPS)
constexpr float MAX_FRAME_STEP_CUTSCENE         = 8.0f;         // 8.0 ticks for cutscene catchup
constexpr float DEFAULT_TIME_SCALE              = 1.0f;
constexpr float DEFAULT_SLOW_TIME_SCALE         = 0.2f;

// 1/30.0f (.rodata:0x00b5ce8c) Combat Timing Offsets
constexpr uintptr_t CONSTANT_30FPS_RODATA_OFFSET = 0x00b5ce8cull;

// Critical Combat Sites for Legion Chain & Sync Attack
constexpr uintptr_t SITE_LEGION_CHAIN_BIND_ESCAPE = 0x10e7c8ull;
constexpr uintptr_t SITE_LEGION_CHAIN_TENSION     = 0x11af30ull;
constexpr uintptr_t SITE_LEGION_CHAIN_SPRING      = 0x11e1e8ull;
constexpr uintptr_t SITE_LEGION_CHAIN_DAMPING     = 0x126558ull;
constexpr uintptr_t SITE_SYNC_ATTACK_CUE          = 0x217f48ull;
constexpr uintptr_t SITE_SYNC_ATTACK_WINDOW       = 0x228224ull;
constexpr uintptr_t SITE_SYNC_ATTACK_OPPORTUNITY  = 0x25d144ull;
constexpr uintptr_t SITE_SYNC_ATTACK_EXPIRE_TIMER = 0x25d470ull;

float get_frame_delta_ms();
float get_target_fps();
void set_target_fps(float fps);

float get_target_rate();
void set_target_rate(float rate);

float get_game_time_scale();
void set_game_time_scale(float scale);

float get_slow_time_scale();
void set_slow_time_scale(float scale);

float get_frame_step();
void set_frame_step(float step);

uint32_t get_whole_ticks();
void set_whole_ticks(uint32_t ticks);

// Synchronizes the engine simulation step with real frame time,
// ensuring combat, animation and physics advance smoothly above 30 FPS.
bool sync_simulation_step(float measured_delta_ms = 0.0f, bool in_cutscene = false);

// Present interval derivation helper (1 for 60+ FPS, 0 for unlimited, 2 for 30 FPS)
int get_target_present_interval(int guest_interval = 2);

// Calculates the effective delta scaling factor for direct 1/30.0f timers
float get_combat_delta_scale(float current_fps = 0.0f);

// Calculates the effective combat window duration in seconds for a given base window in 30 FPS frames
float calculate_combat_window_seconds(float base_window_frames, float current_fps = 0.0f);

// Verifies that combat pacing invariants (Chain Bind & Sync Attack) remain true at arbitrary framerates
bool verify_combat_framerate_invariants(float current_fps = 0.0f);

} // namespace ac::time
