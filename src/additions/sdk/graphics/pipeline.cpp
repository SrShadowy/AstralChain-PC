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
    if (vertex_count <= THRESHOLD_UI_QUAD_VERTICES && instance_count <= 1u) {
        return true;
    }
    if (!depth_test && vertex_count <= THRESHOLD_UI_NODEPTH_COUNT && instance_count <= THRESHOLD_UI_MAX_INSTANCES) {
        return true;
    }
    return false;
}

} // namespace ac::vulkan
