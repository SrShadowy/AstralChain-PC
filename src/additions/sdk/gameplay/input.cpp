#include "input.hpp"

namespace ac::input {

StickVector get_right_stick() {
    StickVector stick = {0.0f, 0.0f};
    ac::memory::read_deref<float>(PAD_INPUT_STATE_OFFSET, OFFSET_STICK_R_X, &stick.x);
    ac::memory::read_deref<float>(PAD_INPUT_STATE_OFFSET, OFFSET_STICK_R_Y, &stick.y);
    return stick;
}

StickVector get_left_stick() {
    StickVector stick = {0.0f, 0.0f};
    ac::memory::read_deref<float>(PAD_INPUT_STATE_OFFSET, OFFSET_STICK_L_X, &stick.x);
    ac::memory::read_deref<float>(PAD_INPUT_STATE_OFFSET, OFFSET_STICK_L_Y, &stick.y);
    return stick;
}

uint32_t get_buttons_held() {
    uint32_t buttons = 0;
    ac::memory::read_deref<uint32_t>(PAD_INPUT_STATE_OFFSET, OFFSET_BUTTONS_HELD, &buttons);
    return buttons;
}

} // namespace ac::input
