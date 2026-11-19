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

// Xbox Glyph Replacement Atlas dimensions & offsets
constexpr int32_t XBOX_ATLAS_WIDTH   = 256;
constexpr int32_t XBOX_ATLAS_HEIGHT  = 512;
constexpr int32_t XBOX_FACE_SIZE     = 22;

struct GlyphRect {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
};

constexpr GlyphRect GLYPH_BUTTON_A     = {181, 97,  XBOX_FACE_SIZE, XBOX_FACE_SIZE};
constexpr GlyphRect GLYPH_BUTTON_B     = {125, 97,  XBOX_FACE_SIZE, XBOX_FACE_SIZE};
constexpr GlyphRect GLYPH_BUTTON_X     = {69,  141, XBOX_FACE_SIZE, XBOX_FACE_SIZE};
constexpr GlyphRect GLYPH_BUTTON_Y     = {9,   141, XBOX_FACE_SIZE, XBOX_FACE_SIZE};
constexpr GlyphRect GLYPH_BUTTON_MINUS = {69,  97,  22,             22};
constexpr GlyphRect GLYPH_BUTTON_PLUS  = {69,  185, 22,             22};
constexpr GlyphRect GLYPH_BUTTON_LB    = {68,  230, 24,             20};
constexpr GlyphRect GLYPH_BUTTON_RB    = {124, 142, 24,             20};
constexpr GlyphRect GLYPH_BUTTON_LT    = {180, 140, 24,             24};
constexpr GlyphRect GLYPH_BUTTON_RT    = {8,   184, 24,             24};

bool is_ui_or_dynamic_draw(
    uint64_t vs_hash,
    uint64_t fs_hash,
    bool depth_test,
    uint32_t vertex_count,
    uint32_t instance_count
);

} // namespace ac::vulkan
