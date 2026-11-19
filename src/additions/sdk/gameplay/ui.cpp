#include "ui.hpp"

namespace ac::ui {

int32_t get_active_menu() {
    int32_t menu_id = MENU_ID_NONE;
    if (!ac::memory::read<int32_t>(ACTIVE_MENU_DIRECT_OFFSET, &menu_id)) {
        return MENU_ID_NONE;
    }
    return menu_id;
}

bool is_menu_open() {
    return get_active_menu() >= 0;
}

bool is_in_gameplay() {
    return get_active_menu() == MENU_ID_NONE;
}

void set_menu_inactive() {
    int32_t none = MENU_ID_NONE;
    ac::memory::write<int32_t>(ACTIVE_MENU_DIRECT_OFFSET, none);
}

bool is_gameplay_paused() {
    uint32_t state = 0;
    if (ac::memory::read_deref<uint32_t>(SLOT_ENGINE_STATE, OFFSET_GAMEPLAY_PAUSED, &state)) {
        if ((state & MASK_GAMEPLAY_PAUSED) != 0) {
            return true;
        }
    }
    return is_menu_open();
}

bool is_camera_frozen() {
    uint8_t freeze = 0;
    if (ac::memory::read_deref<uint8_t>(SLOT_ENGINE_STATE, OFFSET_CAMERA_FREEZE, &freeze)) {
        if ((freeze & MASK_CAMERA_FREEZE_BIT) != 0) {
            return true;
        }
    }
    return false;
}

bool is_engine_transitioning() {
    uint32_t val = 0;
    if (ac::memory::read_deref<uint32_t>(SLOT_ENGINE_STATE, OFFSET_ENGINE_TRANSITION, &val)) {
        return (val >> 29) != 0;
    }
    return false;
}

int32_t get_selected_item_index() {
    int32_t index = -1;
    if (ac::memory::read_deref<int32_t>(SLOT_MENU_SELECTION, OFFSET_SEL_ITEM_INDEX, &index)) {
        return index;
    }
    return -1;
}

uint64_t get_selected_item_id() {
    uint64_t id = 0;
    if (ac::memory::read_deref<uint64_t>(SLOT_MENU_SELECTION, OFFSET_SEL_ITEM_ID, &id)) {
        return id;
    }
    return 0;
}

bool has_selection() {
    return get_selected_item_index() >= 0;
}

} // namespace ac::ui
