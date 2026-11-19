#include <cassert>
#include <iostream>
#include "additions/sdk/ac_sdk.hpp"

int main() {
    // 1. Verify Memory Constants & .cpp functions
    assert(ac::memory::MAIN_MODULE_BASE == 0x7000010000ull);
    assert(ac::memory::get_main_base() == 0x7000010000ull);
    assert(ac::memory::is_valid_guest_ptr(0x7000000000ull, 4) == true);
    assert(ac::memory::is_valid_guest_ptr(0x1000ull, 4) == false);

    // 2. Verify Camera Offsets & Manager Singletons
    assert(ac::camera::VTABLE_OFFSET == 0xd5b3d0ull);
    assert(ac::camera::METHOD_UPDATE_CAMERA == 0x27f640ull);
    assert(ac::camera::FREE_CAMERA_UPDATE == 0x2bd8b0ull);
    assert(ac::camera::SLOT_CAMERA_MANAGER == 0xdb83a0ull);
    assert(ac::camera::CAMERA_MANAGER_DIRECT == 0xdca490ull);
    assert(ac::camera::GET_CAMERA_MANAGER_FN == 0x88fa20ull);
    assert(ac::camera::OFFSET_CAM_MGR_POS == 0x10ull);
    assert(ac::camera::OFFSET_CAM_MGR_TARGET == 0x20ull);
    assert(ac::camera::OFFSET_CAM_MGR_ACTIVE == 0x58ull);
    assert(ac::camera::OFFSET_INPUT_DELTA_X == 0x8d8ull);
    assert(ac::camera::OFFSET_INPUT_DELTA_Y == 0x8e0ull);
    assert(ac::camera::OFFSET_POS_X == 0xb50ull);
    assert(ac::camera::OFFSET_PITCH_ACCUM == 0x1284ull);

    // 3. Verify UI Offsets, Engine Pause & Selection Structs
    assert(ac::ui::ACTIVE_MENU_PTR_SLOT == 0xdbec80ull);
    assert(ac::ui::ACTIVE_MENU_DIRECT_OFFSET == 0xd82d18ull);
    assert(ac::ui::OPEN_MENU_FN == 0x61bf00ull);
    assert(ac::ui::CLOSE_MENU_FN == 0x61bed8ull);
    assert(ac::ui::SLOT_ENGINE_STATE == 0xdb8398ull);
    assert(ac::ui::ENGINE_STATE_DIRECT == 0xdcba30ull);
    assert(ac::ui::OFFSET_CAMERA_FREEZE == 0x13ull);
    assert(ac::ui::OFFSET_GAMEPLAY_PAUSED == 0x30ull);
    assert(ac::ui::SLOT_MENU_SELECTION == 0xdbef78ull);
    assert(ac::ui::MENU_SELECTION_DIRECT == 0xd83180ull);
    assert(ac::ui::OFFSET_SEL_ITEM_INDEX == 0x08ull);
    assert(ac::ui::MENU_ID_NONE == -1);

    // 4. Verify Time Offsets
    assert(ac::time::TIMER_MANAGER_PTR_SLOT == 0xdb89a0ull);
    assert(ac::time::TIMER_MANAGER_DIRECT_OFFSET == 0xdcbfd0ull);
    assert(ac::time::OFFSET_TIME_SCALE_GAME == 0xb0ull);
    assert(ac::time::OFFSET_TARGET_RATE == 0xdcull);
    assert(ac::time::OFFSET_FRAME_DELTA_MS == 0xe0ull);

    // 5. Verify Input Offsets
    assert(ac::input::PAD_INPUT_STATE_OFFSET == 0xdc2540ull);
    assert(ac::input::PAD_STATE_TABLE_OFFSET == 0xdc2518ull);
    assert(ac::input::PAD_SLOT_SIZE == 0xa0ull);
    assert(ac::input::GET_STICK_VECTOR_FN == 0x9ad0a0ull);

    // 6. Verify Game State Offsets, Player Actor & Enums
    assert(ac::game_state::SLOT_SAVE == 0xdb83d8ull);
    assert(ac::game_state::SLOT_SCENE == 0xdb84c8ull);
    assert(ac::game_state::SLOT_PHASE == 0xdb84f8ull);
    assert(ac::game_state::SLOT_SCENARIO == 0xdb8930ull);
    assert(ac::game_state::SLOT_PLAYER_TABLE == 0xdbb680ull);
    assert(ac::game_state::OFFSET_ACTOR_POS_X == 0x50ull);
    assert(ac::game_state::OFFSET_ACTOR_ROT_PITCH == 0x90ull);
    assert(ac::game_state::SLOT_MISSION == 0xdbc420ull);
    assert(ac::game_state::SLOT_BOSS_ACTIVE == 0xdbc928ull);
    assert(ac::game_state::SLOT_AREA == 0xdbcf10ull);
    assert(ac::game_state::SLOT_VOICE_OPT == 0xdc13b0ull);
    assert(static_cast<int32_t>(ac::game_state::VoiceLanguage::Japanese) == 0);
    assert(static_cast<int32_t>(ac::game_state::VoiceLanguage::English) == 1);

    // 7. Verify Graphics Patch Sites & .cpp calculations
    assert(ac::graphics::SVC_SITE_DISPLAY_W == 0x4328c0ull);
    assert(ac::graphics::SVC_SITE_STARTUP_W == 0x40d5e4ull);
    assert(ac::graphics::SVC_SITE_POOL_SIZE == 0x40e0b0ull);
    assert(ac::graphics::SVC_SITE_RENDER_SCALE0 == 0x40f184ull);
    assert(ac::graphics::SVC_SITE_BUFROOT == 0x575a30ull);
    assert(ac::graphics::SVC_SITE_CAMERA_ASPECT == 0x9a30bcull);
    assert(ac::graphics::calculate_pool_budget(1920, 1080) == ac::graphics::RES_POOL_BASE_BYTES);
    assert(ac::graphics::is_widescreen(21.0f / 9.0f) == true);
    assert(ac::graphics::is_widescreen(16.0f / 9.0f) == false);

    // 8. Verify Vulkan Pipeline Hashes, Glyphs & .cpp classification
    assert(ac::vulkan::HASH_UI_BASE_VS == 0xa09bf88abb4a04a7ull);
    assert(ac::vulkan::HASH_VIDEO_FS_0 == 0xf33b4e637319bed0ull);
    assert(ac::vulkan::GLYPH_BUTTON_A.x == 181);
    assert(ac::vulkan::GLYPH_BUTTON_B.x == 125);
    assert(ac::vulkan::is_ui_or_dynamic_draw(ac::vulkan::HASH_UI_BASE_VS, 0, true, 4, 1) == true);
    assert(ac::vulkan::is_ui_or_dynamic_draw(0, 0, true, 500, 1) == false);

    std::cout << "All ac_sdk unit checks passed successfully across all 9 domains with separate .cpp/.hpp translation units!" << std::endl;
    return 0;
}
