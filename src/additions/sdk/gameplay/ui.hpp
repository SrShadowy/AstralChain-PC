#pragma once
#include <cstdint>
#include "../core/memory.hpp"

namespace ac::ui {

// Active Menu Screen Slots & Functions
constexpr uintptr_t ACTIVE_MENU_PTR_SLOT      = 0xdbec80ull;
constexpr uintptr_t ACTIVE_MENU_DIRECT_OFFSET = 0xd82d18ull;
constexpr uintptr_t OPEN_MENU_FN              = 0x61bf00ull;
constexpr uintptr_t CLOSE_MENU_FN             = 0x61bed8ull;

// Engine State, Global Pause & Freeze Bits
constexpr uintptr_t SLOT_ENGINE_STATE         = 0xdb8398ull;
constexpr uintptr_t ENGINE_STATE_DIRECT       = 0xdcba30ull;
constexpr uintptr_t OFFSET_ENGINE_TRANSITION  = 0x00ull;
constexpr uintptr_t OFFSET_CAMERA_FREEZE      = 0x13ull;
constexpr uintptr_t OFFSET_GAMEPLAY_PAUSED    = 0x30ull;

constexpr uint8_t  MASK_CAMERA_FREEZE_BIT     = 1 << 6;  // 0x40
constexpr uint32_t MASK_GAMEPLAY_PAUSED       = 0xe000u; // Pause/Event/Dialog

// Menu Selection & Focused Row Storage
constexpr uintptr_t SLOT_MENU_SELECTION       = 0xdbef78ull;
constexpr uintptr_t MENU_SELECTION_DIRECT     = 0xd83180ull;
constexpr uintptr_t OFFSET_SEL_ITEM_ID        = 0x00ull;
constexpr uintptr_t OFFSET_SEL_ITEM_INDEX     = 0x08ull;
constexpr uintptr_t OFFSET_SEL_ITEM_STATE     = 0x14ull;
constexpr uintptr_t OFFSET_SEL_ITEM_FLAGS     = 0x20ull;

constexpr int32_t MENU_ID_NONE = -1;

int32_t get_active_menu();
bool is_menu_open();
bool is_in_gameplay();
void set_menu_inactive();

// Global engine state queries
bool is_gameplay_paused();
bool is_camera_frozen();
bool is_engine_transitioning();

// UI selection & cursor hit-test queries
int32_t get_selected_item_index();
uint64_t get_selected_item_id();
bool has_selection();

} // namespace ac::ui
