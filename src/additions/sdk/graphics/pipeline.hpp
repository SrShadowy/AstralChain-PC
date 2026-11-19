#pragma once
#include <cstdint>
#include "../core/types.hpp"

namespace ac::vulkan {

// UI and Video Presentation Shader Hashes
constexpr uint64_t HASH_UI_BASE_VS    = 0xa09bf88abb4a04a7ull;
constexpr uint64_t HASH_UI_TILING_VS  = 0x833ab16b25a072acull;
constexpr uint64_t HASH_VIDEO_FS_0    = 0xf33b4e637319bed0ull;
constexpr uint64_t HASH_VIDEO_FS_1    = 0xef0a83385ad50ec7ull;

// Small dynamic draw thresholds
constexpr uint32_t THRESHOLD_UI_QUAD_VERTICES  = 6u;
constexpr uint32_t THRESHOLD_UI_NODEPTH_COUNT  = 64u;
constexpr uint32_t THRESHOLD_UI_MAX_INSTANCES  = 4u;
constexpr uint32_t THRESHOLD_STREAM_COPY_CAP   = 64u * 1024u;

// Glyph Replacement Atlas dimensions & offsets
constexpr int32_t GLYPH_ATLAS_WIDTH   = 256;
constexpr int32_t GLYPH_ATLAS_HEIGHT  = 512;
constexpr int32_t GLYPH_FACE_SIZE     = 22;

struct GlyphRect {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
};

// Standard Xbox One / Series Controller Glyphs
constexpr GlyphRect GLYPH_BUTTON_A     = {181, 97,  GLYPH_FACE_SIZE, GLYPH_FACE_SIZE};
constexpr GlyphRect GLYPH_BUTTON_B     = {125, 97,  GLYPH_FACE_SIZE, GLYPH_FACE_SIZE};
constexpr GlyphRect GLYPH_BUTTON_X     = {69,  141, GLYPH_FACE_SIZE, GLYPH_FACE_SIZE};
constexpr GlyphRect GLYPH_BUTTON_Y     = {9,   141, GLYPH_FACE_SIZE, GLYPH_FACE_SIZE};
constexpr GlyphRect GLYPH_BUTTON_MINUS = {69,  97,  22,              22};
constexpr GlyphRect GLYPH_BUTTON_PLUS  = {69,  185, 22,              22};
constexpr GlyphRect GLYPH_BUTTON_LB    = {68,  230, 24,              20};
constexpr GlyphRect GLYPH_BUTTON_RB    = {124, 142, 24,              20};
constexpr GlyphRect GLYPH_BUTTON_LT    = {180, 140, 24,              24};
constexpr GlyphRect GLYPH_BUTTON_RT    = {8,   184, 24,              24};

// PlayStation DualSense Controller Glyphs
constexpr GlyphRect GLYPH_PS_CROSS     = {181, 193, GLYPH_FACE_SIZE, GLYPH_FACE_SIZE};
constexpr GlyphRect GLYPH_PS_CIRCLE    = {125, 193, GLYPH_FACE_SIZE, GLYPH_FACE_SIZE};
constexpr GlyphRect GLYPH_PS_SQUARE    = {69,  289, GLYPH_FACE_SIZE, GLYPH_FACE_SIZE};
constexpr GlyphRect GLYPH_PS_TRIANGLE  = {9,   289, GLYPH_FACE_SIZE, GLYPH_FACE_SIZE};
constexpr GlyphRect GLYPH_PS_L1        = {68,  338, 24,              20};
constexpr GlyphRect GLYPH_PS_R1        = {124, 338, 24,              20};
constexpr GlyphRect GLYPH_PS_L2        = {180, 338, 24,              24};
constexpr GlyphRect GLYPH_PS_R2        = {8,   338, 24,              24};
constexpr GlyphRect GLYPH_PS_OPTIONS   = {69,  386, 22,              22};
constexpr GlyphRect GLYPH_PS_TOUCHPAD  = {125, 386, 28,              18};

// Keyboard & Mouse Glyphs
constexpr GlyphRect GLYPH_KBM_KEY_SPACE = {8,   434, 40, 20};
constexpr GlyphRect GLYPH_KBM_KEY_SHIFT = {56,  434, 32, 20};
constexpr GlyphRect GLYPH_KBM_KEY_E     = {96,  434, 20, 20};
constexpr GlyphRect GLYPH_KBM_KEY_Q     = {124, 434, 20, 20};
constexpr GlyphRect GLYPH_KBM_MOUSE_LMB = {152, 434, 18, 24};
constexpr GlyphRect GLYPH_KBM_MOUSE_RMB = {178, 434, 18, 24};
constexpr GlyphRect GLYPH_KBM_MOUSE_MMB = {204, 434, 18, 24};

enum class GlyphTheme : uint32_t {
    NintendoSwitch = 0,
    Xbox           = 1,
    PlayStation    = 2,
    KeyboardMouse  = 3
};

constexpr uint32_t BTN_ACTION_DOWN  = 0u; // A / Cross / Space
constexpr uint32_t BTN_ACTION_RIGHT = 1u; // B / Circle / Shift
constexpr uint32_t BTN_ACTION_LEFT  = 2u; // X / Square / Q
constexpr uint32_t BTN_ACTION_UP    = 3u; // Y / Triangle / E
constexpr uint32_t BTN_SHOULDER_L   = 4u; // LB / L1
constexpr uint32_t BTN_SHOULDER_R   = 5u; // RB / R1
constexpr uint32_t BTN_TRIGGER_L    = 6u; // LT / L2 / RMB
constexpr uint32_t BTN_TRIGGER_R    = 7u; // RT / R2 / LMB
constexpr uint32_t BTN_START        = 8u; // Plus / Start / Options
constexpr uint32_t BTN_SELECT       = 9u; // Minus / Back / Touchpad

bool is_ui_or_dynamic_draw(
    uint64_t vs_hash,
    uint64_t fs_hash,
    bool depth_test,
    uint32_t vertex_count,
    uint32_t instance_count
);

GlyphRect get_button_glyph(GlyphTheme theme, uint32_t button_action);

} // namespace ac::vulkan
