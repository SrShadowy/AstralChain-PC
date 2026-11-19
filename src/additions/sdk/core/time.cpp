#include "time.hpp"

namespace ac::time {

float get_frame_delta_ms() {
    float delta_ms = 0.0f;
    if (!ac::memory::read<float>(TIMER_MANAGER_DIRECT_OFFSET + OFFSET_FRAME_DELTA_MS, &delta_ms)) {
        return 0.0f;
    }
    return delta_ms;
}

float get_target_fps() {
    float rate = 0.0f;
    if (!ac::memory::read<float>(TIMER_MANAGER_DIRECT_OFFSET + OFFSET_TARGET_RATE, &rate) || rate <= 0.0f) {
        return 30.0f;
    }
    return (1.0f / rate) * 1000.0f;
}

float get_game_time_scale() {
    float scale = 1.0f;
    if (!ac::memory::read<float>(TIMER_MANAGER_DIRECT_OFFSET + OFFSET_TIME_SCALE_GAME, &scale)) {
        return 1.0f;
    }
    return scale;
}

void set_game_time_scale(float scale) {
    if (scale < 0.0f) {
        scale = 0.0f;
    }
    ac::memory::write<float>(TIMER_MANAGER_DIRECT_OFFSET + OFFSET_TIME_SCALE_GAME, scale);
}

} // namespace ac::time
