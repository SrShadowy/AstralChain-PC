#include "input.hpp"

namespace ac::input {

static uintptr_t get_pad_input_state_slot() {
    static uintptr_t s_slot = 0;
    if (!s_slot) {
        s_slot = ac::memory::resolve_slot_from_aob(ac::memory::AOB_PAD_INPUT_STATE, PAD_INPUT_STATE_OFFSET);
    }
    return s_slot;
}

StickVector get_right_stick() {
    StickVector stick = {0.0f, 0.0f};
    ac::memory::read_deref<float>(get_pad_input_state_slot(), OFFSET_STICK_R_X, &stick.x);
    ac::memory::read_deref<float>(get_pad_input_state_slot(), OFFSET_STICK_R_Y, &stick.y);
    return stick;
}

StickVector get_left_stick() {
    StickVector stick = {0.0f, 0.0f};
    ac::memory::read_deref<float>(get_pad_input_state_slot(), OFFSET_STICK_L_X, &stick.x);
    ac::memory::read_deref<float>(get_pad_input_state_slot(), OFFSET_STICK_L_Y, &stick.y);
    return stick;
}

uint32_t get_buttons_held() {
    uint32_t buttons = 0;
    ac::memory::read_deref<uint32_t>(get_pad_input_state_slot(), OFFSET_BUTTONS_HELD, &buttons);
    return buttons;
}

} // namespace ac::input
