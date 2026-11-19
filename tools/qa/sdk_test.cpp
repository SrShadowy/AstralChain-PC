#include <cassert>
#include <cmath>
#include <iostream>
#include "additions/sdk/ac_sdk.hpp"

int main() {
    // 1. Verify Memory Constants, AOB Signatures & Dynamic Slot Decoder
    assert(ac::memory::MAIN_MODULE_BASE == 0x7000010000ull);
    assert(ac::memory::get_main_base() == 0x7000010000ull);
    assert(ac::memory::is_valid_guest_ptr(0x7000000000ull, 4) == true);
    assert(ac::memory::is_valid_guest_ptr(0x1000ull, 4) == false);

    assert(ac::memory::AOB_GET_CAMERA_MANAGER != nullptr);
    assert(ac::memory::AOB_TIMER_MANAGER != nullptr);
    assert(ac::memory::AOB_ENGINE_STATE != nullptr);
    assert(ac::memory::AOB_SAVE != nullptr);
    assert(ac::memory::AOB_SCENE != nullptr);
    assert(ac::memory::AOB_PHASE != nullptr);
    assert(ac::memory::AOB_SCENARIO != nullptr);
    assert(ac::memory::AOB_PLAYER_TABLE != nullptr);
    assert(ac::memory::AOB_MISSION != nullptr);
    assert(ac::memory::AOB_BOSS_ACTIVE != nullptr);
    assert(ac::memory::AOB_AREA != nullptr);
    assert(ac::memory::AOB_SUBAREA != nullptr);
    assert(ac::memory::AOB_MENU_PTR != nullptr);
    assert(ac::memory::AOB_MENU_SELECTION != nullptr);
    assert(ac::memory::AOB_VOICE_OPT != nullptr);
    assert(ac::memory::AOB_PAD_STATE_TABLE != nullptr);
    assert(ac::memory::AOB_PAD_INPUT_STATE != nullptr);
    assert(ac::memory::AOB_VOICE_SE != nullptr);
    assert(ac::memory::AOB_OPEN_MENU != nullptr);
    assert(ac::memory::AOB_CAMERA_DIRECTOR_UPDATE != nullptr);

    assert(ac::memory::scan_aob(ac::memory::AOB_OPEN_MENU) == 0); // gracefully returns 0 when memory is not mapped
    assert(ac::memory::resolve_slot_from_aob(ac::memory::AOB_GET_CAMERA_MANAGER, 0xdb83a0ull) == 0xdb83a0ull);

    // Verify synthetic ARM64 ADRP + LDR instruction decoding
    uint32_t mock_code[4] = {
        0xb0002940u, // ADRP x0, #page_rel
        0xf941d000u, // LDR  x0, [x0, #0x3a0]
        0xd65f03c0u, // RET
        0x00000000u
    };
    uintptr_t decoded_slot = ac::memory::decode_adrp_ldr(reinterpret_cast<uintptr_t>(mock_code), 4);
    assert(decoded_slot != 0);

    // 2. Verify Camera Offsets, Modes, FOV & Shake
    assert(ac::camera::VTABLE_OFFSET == 0xd5b3d0ull);
    assert(ac::camera::METHOD_UPDATE_CAMERA == 0x27f640ull);
    assert(ac::camera::FREE_CAMERA_UPDATE == 0x2bd8b0ull);
    assert(ac::camera::SLOT_CAMERA_MANAGER == 0xdb83a0ull);
    assert(ac::camera::CAMERA_MANAGER_DIRECT == 0xdca490ull);
    assert(ac::camera::GET_CAMERA_MANAGER_FN == 0x88fa20ull);
    assert(ac::camera::OFFSET_CAM_MGR_POS == 0x10ull);
    assert(ac::camera::OFFSET_CAM_MGR_TARGET == 0x20ull);
    assert(ac::camera::OFFSET_CAM_MGR_ACTIVE == 0x58ull);
    assert(ac::camera::OFFSET_CAMERA_FOV == 0x60ull);
    assert(ac::camera::OFFSET_CAMERA_MODE == 0x644ull);
    assert(ac::camera::OFFSET_CAMERA_SHAKE == 0x658ull);
    assert(ac::camera::OFFSET_INPUT_DELTA_X == 0x8d8ull);
    assert(ac::camera::OFFSET_INPUT_DELTA_Y == 0x8e0ull);
    assert(ac::camera::OFFSET_POS_X == 0xb50ull);
    assert(ac::camera::OFFSET_PITCH_ACCUM == 0x1284ull);
    assert(ac::camera::DEFAULT_FOV_DEGREES == 45.0f);
    assert(ac::camera::CAMERA_MODE_FREE == 1u);
    assert(ac::camera::get_fov(0) == ac::camera::DEFAULT_FOV_DEGREES);
    assert(ac::camera::is_free_camera_enabled(0) == false);
    assert(ac::camera::get_camera_shake_factor(0) == ac::camera::DEFAULT_SHAKE_FACTOR);

    // 3. Verify UI Offsets, Hit-Testing & Selection
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

    float ui_x = 0.0f;
    float ui_y = 0.0f;
    assert(ac::ui::screen_to_ui_coords(960.0f, 540.0f, 1920.0f, 1080.0f, &ui_x, &ui_y) == true);
    assert(std::fabs(ui_x - 960.0f) < 0.001f);
    assert(std::fabs(ui_y - 540.0f) < 0.001f);

    ac::ui::MenuItemRect items[2] = {
        {0, 100.0f, 100.0f, 300.0f, 150.0f},
        {1, 100.0f, 160.0f, 300.0f, 210.0f}
    };
    assert(ac::ui::hit_test_menu_item(200.0f, 120.0f, items, 2) == 0);
    assert(ac::ui::hit_test_menu_item(200.0f, 180.0f, items, 2) == 1);
    assert(ac::ui::hit_test_menu_item(50.0f, 50.0f, items, 2) == -1);

    // 4. Verify Time Offsets, Step Bounds & Simulation Sync
    assert(ac::time::TIMER_MANAGER_PTR_SLOT == 0xdb89a0ull);
    assert(ac::time::TIMER_MANAGER_DIRECT_OFFSET == 0xdcbfd0ull);
    assert(ac::time::OFFSET_TIME_SCALE_GAME == 0xb0ull);
    assert(ac::time::OFFSET_TIME_SCALE_SLOW == 0xb8ull);
    assert(ac::time::OFFSET_FRAME_STEP == 0xd0ull);
    assert(ac::time::OFFSET_REMAINDER_TICKS == 0xd4ull);
    assert(ac::time::OFFSET_WHOLE_TICKS == 0xd8ull);
    assert(ac::time::OFFSET_TARGET_RATE == 0xdcull);
    assert(ac::time::OFFSET_FRAME_DELTA_MS == 0xe0ull);
    assert(ac::time::BASE_SIMULATION_FPS == 30.0f);
    assert(ac::time::MIN_FRAME_STEP_FLOOR == 1.0f / 16.0f);
    assert(ac::time::MAX_FRAME_STEP_GAMEPLAY == 2.0f);
    assert(ac::time::MAX_FRAME_STEP_CUTSCENE == 8.0f);
    assert(ac::time::get_target_fps() == 30.0f);
    assert(ac::time::get_game_time_scale() == 1.0f);
    assert(ac::time::get_slow_time_scale() == 0.2f);
    assert(ac::time::get_frame_step() == 1.0f);
    assert(ac::time::get_whole_ticks() == 0);
    assert(ac::time::sync_simulation_step(16.6667f, false) == false);
    assert(ac::time::get_target_present_interval(2) == 1);

    // Verify 1/30.0f Combat Sites & Timing Invariants
    assert(ac::time::CONSTANT_30FPS_RODATA_OFFSET == 0x00b5ce8cull);
    assert(ac::time::SITE_LEGION_CHAIN_BIND_ESCAPE == 0x10e7c8ull);
    assert(ac::time::SITE_LEGION_CHAIN_TENSION == 0x11af30ull);
    assert(ac::time::SITE_LEGION_CHAIN_SPRING == 0x11e1e8ull);
    assert(ac::time::SITE_LEGION_CHAIN_DAMPING == 0x126558ull);
    assert(ac::time::SITE_SYNC_ATTACK_CUE == 0x217f48ull);
    assert(ac::time::SITE_SYNC_ATTACK_WINDOW == 0x228224ull);
    assert(ac::time::SITE_SYNC_ATTACK_OPPORTUNITY == 0x25d144ull);
    assert(ac::time::SITE_SYNC_ATTACK_EXPIRE_TIMER == 0x25d470ull);

    assert(std::fabs(ac::time::get_combat_delta_scale(30.0f) - 1.0f) < 0.001f);
    assert(std::fabs(ac::time::get_combat_delta_scale(60.0f) - 0.5f) < 0.001f);
    assert(std::fabs(ac::time::get_combat_delta_scale(120.0f) - 0.25f) < 0.001f);
    assert(std::fabs(ac::time::calculate_combat_window_seconds(30.0f, 30.0f) - 1.0f) < 0.001f);
    assert(std::fabs(ac::time::calculate_combat_window_seconds(30.0f, 120.0f) - 1.0f) < 0.001f);
    assert(ac::time::verify_combat_framerate_invariants(30.0f) == true);
    assert(ac::time::verify_combat_framerate_invariants(60.0f) == true);
    assert(ac::time::verify_combat_framerate_invariants(120.0f) == true);
    assert(ac::time::verify_combat_framerate_invariants(144.0f) == true);

    // 5. Verify Input Offsets
    assert(ac::input::PAD_INPUT_STATE_OFFSET == 0xdc2540ull);
    assert(ac::input::PAD_STATE_TABLE_OFFSET == 0xdc2518ull);
    assert(ac::input::PAD_SLOT_SIZE == 0xa0ull);
    assert(ac::input::GET_STICK_VECTOR_FN == 0x9ad0a0ull);

    // 6. Verify Game State Offsets, Player HP, Energy & Transforms
    assert(ac::game_state::SLOT_SAVE == 0xdb83d8ull);
    assert(ac::game_state::SLOT_SCENE == 0xdb84c8ull);
    assert(ac::game_state::SLOT_PHASE == 0xdb84f8ull);
    assert(ac::game_state::SLOT_SCENARIO == 0xdb8930ull);
    assert(ac::game_state::SLOT_PLAYER_TABLE == 0xdbb680ull);
    assert(ac::game_state::OFFSET_ACTOR_POS_X == 0x50ull);
    assert(ac::game_state::OFFSET_ACTOR_ROT_PITCH == 0x90ull);
    assert(ac::game_state::OFFSET_ACTOR_DIRECT_HP == 0x840ull);
    assert(ac::game_state::OFFSET_ACTOR_DIRECT_MAX_HP == 0x844ull);
    assert(ac::game_state::OFFSET_ACTOR_LEGION_ENERGY == 0xd54ull);
    assert(ac::game_state::OFFSET_ACTOR_LEGION_MAX_ENERGY == 0xd58ull);
    assert(ac::game_state::SLOT_HUD_LEGION == 0xdb8580ull);
    assert(ac::game_state::DEFAULT_PLAYER_MAX_HP == 1000);
    assert(ac::game_state::DEFAULT_LEGION_MAX_ENERGY == 100.0f);
    assert(ac::game_state::SLOT_MISSION == 0xdbc420ull);
    assert(ac::game_state::SLOT_BOSS_ACTIVE == 0xdbc928ull);
    assert(ac::game_state::SLOT_AREA == 0xdbcf10ull);
    assert(ac::game_state::SLOT_VOICE_OPT == 0xdc13b0ull);
    assert(static_cast<int32_t>(ac::game_state::VoiceLanguage::Japanese) == 0);
    assert(static_cast<int32_t>(ac::game_state::VoiceLanguage::English) == 1);

    ac::Vec3 dummy_pos = {10.0f, 20.0f, 30.0f};
    assert(ac::game_state::set_player_position(dummy_pos, 0) == false);

    // 7. Verify Graphics Patch Sites, Draw Distance & Post-Processing
    assert(ac::graphics::SVC_SITE_DISPLAY_W == 0x4328c0ull);
    assert(ac::graphics::SVC_SITE_STARTUP_W == 0x40d5e4ull);
    assert(ac::graphics::SVC_SITE_POOL_SIZE == 0x40e0b0ull);
    assert(ac::graphics::SVC_SITE_RENDER_SCALE0 == 0x40f184ull);
    assert(ac::graphics::SVC_SITE_BUFROOT == 0x575a30ull);
    assert(ac::graphics::SVC_SITE_CAMERA_ASPECT == 0x9a30bcull);
    assert(ac::graphics::SITE_DRAW_DIST_ENTITY0 == 0x551df0ull);
    assert(ac::graphics::SITE_DRAW_DIST_TERRAIN == 0x59baa0ull);
    assert(ac::graphics::calculate_pool_budget(1920, 1080) == ac::graphics::RES_POOL_BASE_BYTES);
    assert(ac::graphics::is_widescreen(21.0f / 9.0f) == true);
    assert(ac::graphics::is_widescreen(16.0f / 9.0f) == false);
    assert(ac::graphics::get_draw_distance_multiplier(ac::graphics::DrawDistancePreset::Low) == 0.75f);
    assert(ac::graphics::get_draw_distance_multiplier(ac::graphics::DrawDistancePreset::Ultra) == 2.5f);
    assert(ac::graphics::is_post_process_pass_enabled(ac::graphics::HASH_FILTER_MOTION_BLUR_CAM, false, true, true) == false);
    assert(ac::graphics::is_post_process_pass_enabled(ac::graphics::HASH_FILTER_CHROMATIC_ABERR, true, false, true) == false);
    assert(ac::graphics::is_post_process_pass_enabled(ac::graphics::HASH_FILTER_FILM_GRAIN, true, true, false) == false);

    // 8. Verify Vulkan Pipeline Hashes, Multi-platform Glyphs & Themes
    assert(ac::vulkan::HASH_UI_BASE_VS == 0xa09bf88abb4a04a7ull);
    assert(ac::vulkan::HASH_VIDEO_FS_0 == 0xf33b4e637319bed0ull);
    assert(ac::vulkan::GLYPH_BUTTON_A.x == 181);
    assert(ac::vulkan::GLYPH_PS_CROSS.x == 181);
    assert(ac::vulkan::GLYPH_PS_CROSS.y == 193);
    assert(ac::vulkan::GLYPH_KBM_KEY_SPACE.width == 40);
    assert(ac::vulkan::is_ui_or_dynamic_draw(ac::vulkan::HASH_UI_BASE_VS, 0, true, 4, 1) == true);
    assert(ac::vulkan::is_ui_or_dynamic_draw(0, 0, true, 500, 1) == false);

    ac::vulkan::GlyphRect ps_glyph = ac::vulkan::get_button_glyph(ac::vulkan::GlyphTheme::PlayStation, ac::vulkan::BTN_ACTION_DOWN);
    assert(ps_glyph.x == ac::vulkan::GLYPH_PS_CROSS.x && ps_glyph.y == ac::vulkan::GLYPH_PS_CROSS.y);
    ac::vulkan::GlyphRect kbm_glyph = ac::vulkan::get_button_glyph(ac::vulkan::GlyphTheme::KeyboardMouse, ac::vulkan::BTN_ACTION_DOWN);
    assert(kbm_glyph.width == ac::vulkan::GLYPH_KBM_KEY_SPACE.width);

    // 9. Verify Sound Subsystem Controls & Focus-loss Invariants
    assert(ac::sound::VOLUME_MIN_PCT == 0);
    assert(ac::sound::VOLUME_MAX_PCT == 100);
    ac::sound::set_master_volume(75);
    assert(ac::sound::get_master_volume() == 75);
    ac::sound::set_master_volume(150); // should clamp
    assert(ac::sound::get_master_volume() == 100);
    ac::sound::set_bgm_volume(60);
    assert(ac::sound::get_bgm_volume() == 60);

    ac::sound::set_mute_on_unfocus(true);
    assert(ac::sound::is_mute_on_unfocus() == true);
    float gain_focused = ac::sound::calculate_effective_gain(80, true, true);
    float gain_unfocused = ac::sound::calculate_effective_gain(80, false, true);
    float gain_unfocused_nomute = ac::sound::calculate_effective_gain(80, false, false);
    assert(std::fabs(gain_focused - 0.80f) < 0.001f);
    assert(std::fabs(gain_unfocused - 0.0f) < 0.001f);
    assert(std::fabs(gain_unfocused_nomute - 0.80f) < 0.001f);

    // 10. Verify Text Language & Localization Invariants
    assert(ac::game_state::SLOT_TEXT_LANG_INDEX == 0xdc26f8ull);
    assert(ac::game_state::SLOT_TEXT_LANG_DESIRED == 0xdc2750ull);
    assert(ac::game_state::GET_TEXT_LANGUAGE_FN == 0x9c2e70ull);
    assert(static_cast<int32_t>(ac::game_state::TextLanguage::Japanese) == 0);
    assert(static_cast<int32_t>(ac::game_state::TextLanguage::AmericanEnglish) == 1);
    assert(static_cast<int32_t>(ac::game_state::TextLanguage::Portuguese) == 10);
    assert(static_cast<int32_t>(ac::game_state::TextLanguage::LatinAmericanSpanish) == 14);

    // 11. Verify Mod Management, Discovery, Category & Enable/Disable
    auto all_mods = ac::mods::list_mods();
    assert(!all_mods.empty()); // mods/traducao_ptbr is present
    auto pt_mod = ac::mods::get_mod_info("traducao_ptbr");
    assert(pt_mod.has_value());
    assert(pt_mod->category == ac::mods::ModCategory::Translation);
    assert(pt_mod->has_romfs == true);
    assert(ac::mods::is_mod_enabled("traducao_ptbr") == true);
    assert(ac::mods::ac_mods_is_enabled("traducao_ptbr") == 1);
    assert(ac::mods::has_active_translation_mod() == true);
    assert(ac::game_state::is_translation_mod_active_for_language(ac::game_state::TextLanguage::Portuguese) == true);

    // Test toggle / disable / enable cycle
    bool disable_ok = ac::mods::set_mod_enabled("traducao_ptbr", false);
    assert(disable_ok);
    assert(ac::mods::is_mod_enabled("traducao_ptbr") == false);
    assert(ac::mods::has_active_translation_mod() == false);
    assert(ac::game_state::is_translation_mod_active_for_language(ac::game_state::TextLanguage::Portuguese) == false);

    bool reenable_ok = ac::mods::set_mod_enabled("traducao_ptbr", true);
    assert(reenable_ok);
    assert(ac::mods::is_mod_enabled("traducao_ptbr") == true);
    assert(ac::mods::has_active_translation_mod() == true);
    assert(ac::game_state::is_translation_mod_active_for_language(ac::game_state::TextLanguage::Portuguese) == true);

    auto trans_mods = ac::mods::get_translation_mods();
    assert(!trans_mods.empty());
    assert(std::string(ac::mods::category_to_string(ac::mods::ModCategory::Translation)) == "Translation");

    std::cout << "All ac_sdk unit checks passed successfully across all 11 domain modules with complete .cpp/.hpp translation units!" << std::endl;
    return 0;
}
