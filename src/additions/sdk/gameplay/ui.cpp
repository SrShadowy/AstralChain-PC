#include "ui.hpp"

namespace ac::ui {

static uintptr_t get_slot_engine_state() {
    static uintptr_t s_slot = 0;
    if (!s_slot) {
        s_slot = ac::memory::resolve_slot_from_aob(ac::memory::AOB_ENGINE_STATE, SLOT_ENGINE_STATE);
    }
    return s_slot;
}

static uintptr_t get_slot_menu_selection() {
    static uintptr_t s_slot = 0;
    if (!s_slot) {
        s_slot = ac::memory::resolve_slot_from_aob(ac::memory::AOB_MENU_SELECTION, SLOT_MENU_SELECTION);
    }
    return s_slot;
}

static uintptr_t get_slot_menu_ptr() {
    static uintptr_t s_slot = 0;
    if (!s_slot) {
        s_slot = ac::memory::resolve_slot_from_aob(ac::memory::AOB_MENU_PTR, ACTIVE_MENU_PTR_SLOT);
    }
    return s_slot;
}

int32_t get_active_menu() {
    int32_t menu_id = MENU_ID_NONE;
    if (ac::memory::read_deref<int32_t>(get_slot_menu_ptr(), 0, &menu_id)) {
        return menu_id;
    }
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
    if (ac::memory::read_deref<uint32_t>(get_slot_engine_state(), OFFSET_GAMEPLAY_PAUSED, &state)) {
        if ((state & MASK_GAMEPLAY_PAUSED) != 0) {
            return true;
        }
    }
    return is_menu_open();
}

bool is_camera_frozen() {
    uint8_t freeze = 0;
    if (ac::memory::read_deref<uint8_t>(get_slot_engine_state(), OFFSET_CAMERA_FREEZE, &freeze)) {
        if ((freeze & MASK_CAMERA_FREEZE_BIT) != 0) {
            return true;
        }
    }
    return false;
}

bool is_engine_transitioning() {
    uint32_t val = 0;
    if (ac::memory::read_deref<uint32_t>(get_slot_engine_state(), OFFSET_ENGINE_TRANSITION, &val)) {
        return (val >> 29) != 0;
    }
    return false;
}

int32_t get_selected_item_index() {
    int32_t index = -1;
    if (ac::memory::read_deref<int32_t>(get_slot_menu_selection(), OFFSET_SEL_ITEM_INDEX, &index)) {
        return index;
    }
    return -1;
}

uint64_t get_selected_item_id() {
    uint64_t id = 0;
    if (ac::memory::read_deref<uint64_t>(get_slot_menu_selection(), OFFSET_SEL_ITEM_ID, &id)) {
        return id;
    }
    return 0;
}

bool has_selection() {
    return get_selected_item_index() >= 0;
}

bool set_selected_item_index(int32_t index) {
    if (ac::memory::write_deref<int32_t>(get_slot_menu_selection(), OFFSET_SEL_ITEM_INDEX, index)) {
        return true;
    }
    return ac::memory::write<int32_t>(MENU_SELECTION_DIRECT + OFFSET_SEL_ITEM_INDEX, index);
}

bool set_selected_item_id(uint64_t id) {
    if (ac::memory::write_deref<uint64_t>(get_slot_menu_selection(), OFFSET_SEL_ITEM_ID, id)) {
        return true;
    }
    return ac::memory::write<uint64_t>(MENU_SELECTION_DIRECT + OFFSET_SEL_ITEM_ID, id);
}

bool screen_to_ui_coords(float screen_x, float screen_y, float screen_w, float screen_h, float* out_ui_x, float* out_ui_y) {
    if (!out_ui_x || !out_ui_y || screen_w <= 0.0f || screen_h <= 0.0f) {
        return false;
    }
    *out_ui_x = (screen_x / screen_w) * UI_CANVAS_WIDTH;
    *out_ui_y = (screen_y / screen_h) * UI_CANVAS_HEIGHT;
    return true;
}

int32_t hit_test_menu_item(float ui_x, float ui_y, const MenuItemRect* items, size_t count) {
    if (!items || count == 0) {
        return -1;
    }
    for (size_t i = 0; i < count; ++i) {
        const MenuItemRect& r = items[i];
        if (ui_x >= r.x0 && ui_x <= r.x1 && ui_y >= r.y0 && ui_y <= r.y1) {
            return r.index;
        }
    }
    return -1;
}

} // namespace ac::ui
