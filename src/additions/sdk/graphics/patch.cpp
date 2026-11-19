#include "patch.hpp"

namespace ac::graphics {

void snap_grid_dimensions(int32_t in_w, int32_t min_w, int32_t max_w, int32_t* out_w, int32_t* out_h) {
    if (!out_w || !out_h) {
        return;
    }
    int32_t units = (in_w + (RES_GRID_STEP_W / 2)) / RES_GRID_STEP_W;
    int32_t min_u = min_w / RES_GRID_STEP_W;
    int32_t max_u = max_w / RES_GRID_STEP_W;
    if (units < min_u) units = min_u;
    if (units > max_u) units = max_u;
    *out_w = units * RES_GRID_STEP_W;
    *out_h = units * RES_GRID_STEP_H;
}

uint32_t calculate_pool_budget(int32_t w, int32_t h) {
    uint64_t px = static_cast<uint64_t>(w) * static_cast<uint64_t>(h);
    if (px <= RES_1080P_PIXELS) {
        return RES_POOL_BASE_BYTES;
    }
    uint64_t estimated = ((RES_POOL_FIXED_BYTES + px * RES_POOL_BYTES_PER_PX) * 115ull) / 100ull;
    uint64_t aligned = (estimated + MIB_ALIGN_MASK) & ~MIB_ALIGN_MASK;
    return static_cast<uint32_t>(aligned);
}

uint32_t calculate_gbuffer_extra(int32_t w, int32_t h) {
    uint64_t px = static_cast<uint64_t>(w) * static_cast<uint64_t>(h);
    if (px <= RES_1080P_PIXELS) {
        return 0;
    }
    uint64_t extra = ((px - RES_1080P_PIXELS) * RES_GBUF_BYTES_PER_PX + MIB_ALIGN_MASK) & ~MIB_ALIGN_MASK;
    return static_cast<uint32_t>(extra);
}

bool is_widescreen(float aspect) {
    constexpr float WIDESCREEN_TOLERANCE = 1.01f;
    return aspect > (ASPECT_16_9 * WIDESCREEN_TOLERANCE);
}

} // namespace ac::graphics
