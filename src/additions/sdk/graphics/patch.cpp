#include "patch.hpp"
#include <algorithm>

namespace ac::graphics {

void snap_grid_dimensions(int32_t in_w, int32_t min_w, int32_t max_w, int32_t* out_w, int32_t* out_h) {
    if (!out_w || !out_h) {
        return;
    }

    int32_t clamped_w = std::clamp(in_w, min_w, max_w);
    int32_t snapped_w = (clamped_w / RES_GRID_STEP_W) * RES_GRID_STEP_W;
    int32_t snapped_h = (snapped_w * RES_GRID_STEP_H) / RES_GRID_STEP_W;

    *out_w = snapped_w;
    *out_h = snapped_h;
}

uint32_t calculate_pool_budget(int32_t w, int32_t h) {
    uint64_t pixels = static_cast<uint64_t>(w) * static_cast<uint64_t>(h);
    if (pixels <= RES_1080P_PIXELS) {
        return RES_POOL_BASE_BYTES;
    }

    uint64_t variable_bytes = (pixels * RES_POOL_BYTES_PER_PX);
    uint64_t total = RES_POOL_FIXED_BYTES + variable_bytes;
    uint64_t aligned = (total + MIB_ALIGN_MASK) & ~MIB_ALIGN_MASK;
    return static_cast<uint32_t>(aligned);
}

uint32_t calculate_gbuffer_extra(int32_t w, int32_t h) {
    uint64_t pixels = static_cast<uint64_t>(w) * static_cast<uint64_t>(h);
    if (pixels <= RES_1080P_PIXELS) {
        return 0;
    }
    uint64_t extra_px = pixels - RES_1080P_PIXELS;
    return static_cast<uint32_t>(extra_px * RES_GBUF_BYTES_PER_PX);
}

bool is_widescreen(float aspect) {
    return aspect > (ASPECT_16_9 + 0.01f);
}

float get_draw_distance_multiplier(DrawDistancePreset preset) {
    switch (preset) {
        case DrawDistancePreset::Low:
            return 0.75f;
        case DrawDistancePreset::Medium:
            return 1.0f;
        case DrawDistancePreset::High:
            return 1.5f;
        case DrawDistancePreset::Ultra:
            return 2.5f;
        default:
            return 1.0f;
    }
}

bool is_post_process_pass_enabled(
    uint64_t filter_hash,
    bool motion_blur_enabled,
    bool chrom_aberr_enabled,
    bool grain_enabled
) {
    if (filter_hash == HASH_FILTER_MOTION_BLUR_CAM || filter_hash == HASH_FILTER_MOTION_BLUR_DIR) {
        return motion_blur_enabled;
    }
    if (filter_hash == HASH_FILTER_CHROMATIC_ABERR) {
        return chrom_aberr_enabled;
    }
    if (filter_hash == HASH_FILTER_FILM_GRAIN) {
        return grain_enabled;
    }
    return true;
}

} // namespace ac::graphics
