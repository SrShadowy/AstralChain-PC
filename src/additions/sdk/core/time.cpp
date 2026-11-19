#include "time.hpp"
#include <algorithm>
#include <cmath>

extern "C" {
__attribute__((weak)) int extra_target_present_interval(int guest_interval) {
    if (guest_interval > 1) {
        return 1;
    }
    return guest_interval;
}
}

namespace ac::time {

static uintptr_t get_timer_manager_base() {
    static uintptr_t s_slot = 0;
    if (!s_slot) {
        s_slot = ac::memory::resolve_slot_from_aob(ac::memory::AOB_TIMER_MANAGER, TIMER_MANAGER_PTR_SLOT);
    }
    uintptr_t ptr = 0;
    if (ac::memory::read<uintptr_t>(s_slot, &ptr) && ptr >= ac::memory::MAIN_MODULE_BASE) {
        return ptr - ac::memory::MAIN_MODULE_BASE;
    }
    return TIMER_MANAGER_DIRECT_OFFSET;
}

float get_frame_delta_ms() {
    float delta_ms = 0.0f;
    if (!ac::memory::read<float>(get_timer_manager_base() + OFFSET_FRAME_DELTA_MS, &delta_ms)) {
        return 0.0f;
    }
    return delta_ms;
}

float get_target_fps() {
    float rate = 0.0f;
    if (!ac::memory::read<float>(get_timer_manager_base() + OFFSET_TARGET_RATE, &rate) || rate <= 0.0f) {
        return BASE_SIMULATION_FPS;
    }
    return rate * 1000.0f;
}

void set_target_fps(float fps) {
    if (fps <= 0.0f) {
        fps = BASE_SIMULATION_FPS;
    }
    float rate = fps / 1000.0f;
    ac::memory::write<float>(get_timer_manager_base() + OFFSET_TARGET_RATE, rate);
}

float get_target_rate() {
    float rate = 0.0f;
    if (!ac::memory::read<float>(get_timer_manager_base() + OFFSET_TARGET_RATE, &rate) || rate <= 0.0f) {
        return BASE_SIMULATION_FPS / 1000.0f;
    }
    return rate;
}

void set_target_rate(float rate) {
    if (rate <= 0.0f) {
        rate = BASE_SIMULATION_FPS / 1000.0f;
    }
    ac::memory::write<float>(get_timer_manager_base() + OFFSET_TARGET_RATE, rate);
}

float get_game_time_scale() {
    float scale = DEFAULT_TIME_SCALE;
    if (!ac::memory::read<float>(get_timer_manager_base() + OFFSET_TIME_SCALE_GAME, &scale)) {
        return DEFAULT_TIME_SCALE;
    }
    return scale;
}

void set_game_time_scale(float scale) {
    if (scale < 0.0f) {
        scale = 0.0f;
    }
    ac::memory::write<float>(get_timer_manager_base() + OFFSET_TIME_SCALE_GAME, scale);
}

float get_slow_time_scale() {
    float scale = DEFAULT_SLOW_TIME_SCALE;
    if (!ac::memory::read<float>(get_timer_manager_base() + OFFSET_TIME_SCALE_SLOW, &scale)) {
        return DEFAULT_SLOW_TIME_SCALE;
    }
    return scale;
}

void set_slow_time_scale(float scale) {
    if (scale < 0.0f) {
        scale = 0.0f;
    }
    ac::memory::write<float>(get_timer_manager_base() + OFFSET_TIME_SCALE_SLOW, scale);
}

float get_frame_step() {
    float step = 1.0f;
    if (!ac::memory::read<float>(get_timer_manager_base() + OFFSET_FRAME_STEP, &step)) {
        return 1.0f;
    }
    return step;
}

void set_frame_step(float step) {
    ac::memory::write<float>(get_timer_manager_base() + OFFSET_FRAME_STEP, step);
}

uint32_t get_whole_ticks() {
    uint32_t ticks = 0;
    if (!ac::memory::read<uint32_t>(get_timer_manager_base() + OFFSET_WHOLE_TICKS, &ticks)) {
        return 0;
    }
    return ticks;
}

void set_whole_ticks(uint32_t ticks) {
    ac::memory::write<uint32_t>(get_timer_manager_base() + OFFSET_WHOLE_TICKS, ticks);
}

bool sync_simulation_step(float measured_delta_ms, bool in_cutscene) {
    if (!ac::memory::is_guest_memory_accessible()) {
        return false;
    }

    float dt = measured_delta_ms;
    if (dt <= 0.0f) {
        dt = get_frame_delta_ms();
    }
    if (dt <= 0.0f) {
        float fps = get_target_fps();
        dt = (fps > 0.0f) ? (1000.0f / fps) : (1000.0f / BASE_SIMULATION_FPS);
    }

    float scale = get_game_time_scale();
    float step = dt * (BASE_SIMULATION_FPS / 1000.0f) * scale;
    float max_step = in_cutscene ? MAX_FRAME_STEP_CUTSCENE : MAX_FRAME_STEP_GAMEPLAY;
    step = std::clamp(step, MIN_FRAME_STEP_FLOOR, max_step);

    set_frame_step(step);

    float remainder = 0.0f;
    ac::memory::read<float>(get_timer_manager_base() + OFFSET_REMAINDER_TICKS, &remainder);
    remainder += step;

    uint32_t whole = static_cast<uint32_t>(remainder);
    remainder -= static_cast<float>(whole);

    ac::memory::write<float>(get_timer_manager_base() + OFFSET_REMAINDER_TICKS, remainder);
    set_whole_ticks(whole);
    set_target_rate(BASE_SIMULATION_FPS / 1000.0f);
    return true;
}

int get_target_present_interval(int guest_interval) {
    return extra_target_present_interval(guest_interval);
}

float get_combat_delta_scale(float current_fps) {
    float fps = (current_fps > 0.0f) ? current_fps : get_target_fps();
    if (fps <= 0.0f) {
        fps = BASE_SIMULATION_FPS;
    }
    return BASE_SIMULATION_FPS / fps;
}

float calculate_combat_window_seconds(float base_window_frames, float current_fps) {
    if (base_window_frames <= 0.0f) {
        return 0.0f;
    }
    (void)current_fps;
    // With whole_ticks sub-stepping, nominal frames advance at 30 Hz
    return base_window_frames * (1.0f / BASE_SIMULATION_FPS);
}

bool verify_combat_framerate_invariants(float current_fps) {
    float fps = (current_fps > 0.0f) ? current_fps : get_target_fps();
    if (fps <= 0.0f) {
        fps = BASE_SIMULATION_FPS;
    }
    float dt_ms = 1000.0f / fps;
    float step = dt_ms * (BASE_SIMULATION_FPS / 1000.0f);

    constexpr float ONE_OVER_30 = 1.0f / 30.0f;
    float computed_dt_sec = step * ONE_OVER_30;
    float expected_dt_sec = dt_ms / 1000.0f;
    if (std::fabs(computed_dt_sec - expected_dt_sec) > 0.0001f) {
        return false;
    }

    float window_sec = calculate_combat_window_seconds(30.0f, fps);
    if (std::fabs(window_sec - 1.0f) > 0.0001f) {
        return false;
    }

    return true;
}

} // namespace ac::time
