#include "pipeline.hpp"

namespace ac::vulkan {

bool is_ui_or_dynamic_draw(
    uint64_t vs_hash,
    uint64_t fs_hash,
    bool depth_test,
    uint32_t vertex_count,
    uint32_t instance_count
) {
    if (vs_hash == HASH_UI_BASE_VS || vs_hash == HASH_UI_TILING_VS) {
        return true;
    }
    if (fs_hash == HASH_VIDEO_FS_0 || fs_hash == HASH_VIDEO_FS_1) {
        return true;
    }
    if (!depth_test && vertex_count <= THRESHOLD_UI_NODEPTH_COUNT) {
        return true;
    }
    if (vertex_count <= THRESHOLD_UI_QUAD_VERTICES && instance_count <= THRESHOLD_UI_MAX_INSTANCES) {
        return true;
    }
    return false;
}

GlyphRect get_button_glyph(GlyphTheme theme, uint32_t button_action) {
    if (theme == GlyphTheme::PlayStation) {
        switch (button_action) {
            case BTN_ACTION_DOWN:
                return GLYPH_PS_CROSS;
            case BTN_ACTION_RIGHT:
                return GLYPH_PS_CIRCLE;
            case BTN_ACTION_LEFT:
                return GLYPH_PS_SQUARE;
            case BTN_ACTION_UP:
                return GLYPH_PS_TRIANGLE;
            case BTN_SHOULDER_L:
                return GLYPH_PS_L1;
            case BTN_SHOULDER_R:
                return GLYPH_PS_R1;
            case BTN_TRIGGER_L:
                return GLYPH_PS_L2;
            case BTN_TRIGGER_R:
                return GLYPH_PS_R2;
            case BTN_START:
                return GLYPH_PS_OPTIONS;
            case BTN_SELECT:
                return GLYPH_PS_TOUCHPAD;
            default:
                return GLYPH_PS_CROSS;
        }
    }

    if (theme == GlyphTheme::KeyboardMouse) {
        switch (button_action) {
            case BTN_ACTION_DOWN:
                return GLYPH_KBM_KEY_SPACE;
            case BTN_ACTION_RIGHT:
                return GLYPH_KBM_KEY_SHIFT;
            case BTN_ACTION_LEFT:
                return GLYPH_KBM_KEY_Q;
            case BTN_ACTION_UP:
                return GLYPH_KBM_KEY_E;
            case BTN_TRIGGER_L:
                return GLYPH_KBM_MOUSE_RMB;
            case BTN_TRIGGER_R:
                return GLYPH_KBM_MOUSE_LMB;
            default:
                return GLYPH_KBM_KEY_SPACE;
        }
    }

    // Default: Xbox One / Series theme
    switch (button_action) {
        case BTN_ACTION_DOWN:
            return GLYPH_BUTTON_A;
        case BTN_ACTION_RIGHT:
            return GLYPH_BUTTON_B;
        case BTN_ACTION_LEFT:
            return GLYPH_BUTTON_X;
        case BTN_ACTION_UP:
            return GLYPH_BUTTON_Y;
        case BTN_SHOULDER_L:
            return GLYPH_BUTTON_LB;
        case BTN_SHOULDER_R:
            return GLYPH_BUTTON_RB;
        case BTN_TRIGGER_L:
            return GLYPH_BUTTON_LT;
        case BTN_TRIGGER_R:
            return GLYPH_BUTTON_RT;
        case BTN_START:
            return GLYPH_BUTTON_PLUS;
        case BTN_SELECT:
            return GLYPH_BUTTON_MINUS;
        default:
            return GLYPH_BUTTON_A;
    }
}

} // namespace ac::vulkan
