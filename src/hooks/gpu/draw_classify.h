#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/* UI and Presentation Shader Hashes */
#define AC_UI_BASE_VS_HASH        0xa09bf88abb4a04a7ull
#define AC_UI_TILING_VS_HASH      0x833ab16b25a072acull
#define AC_VIDEO_FS_HASH_0        0xf33b4e637319bed0ull
#define AC_VIDEO_FS_HASH_1        0xef0a83385ad50ec7ull

/* Classification thresholds for small dynamic draws */
#define AC_UI_QUAD_VERTEX_MAX     6u
#define AC_UI_NODEPTH_COUNT_MAX   64u
#define AC_UI_MAX_INSTANCES       4u
#define AC_STREAM_COPY_CAP_BYTES  (64u * 1024u)

/* Returns true if the draw belongs to UI, HUD, IRIS visor widgets,
 * movie playback quads, or small un-depth-tested dynamic 2D overlays.
 * These require host snapshots to avoid host/GPU race conditions.
 * 3D gameplay meshes (with depth test enabled) return false and take
 * the direct VRAM / vcache pathway. */
static inline bool extra_is_ui_or_dynamic(
    uint64_t vs_hash,
    uint64_t fs_hash,
    bool depth_test,
    uint32_t count,
    uint32_t instances
) {
    if (vs_hash == AC_UI_BASE_VS_HASH || vs_hash == AC_UI_TILING_VS_HASH) {
        return true;
    }
    if (fs_hash == AC_VIDEO_FS_HASH_0 || fs_hash == AC_VIDEO_FS_HASH_1) {
        return true;
    }
    if (count <= AC_UI_QUAD_VERTEX_MAX && instances <= 1u) {
        return true;
    }
    if (!depth_test && count <= AC_UI_NODEPTH_COUNT_MAX && instances <= AC_UI_MAX_INSTANCES) {
        return true;
    }
    return false;
}

/* User override via LARECOMP_VK_COPY_SMALL:
 * Negative / unset = automatic semantic classification (UI vs 3D).
 * Non-negative = force count-based copy threshold. */
static inline int64_t extra_copy_small_override(void) {
    static int64_t s_override = -2;
    if (s_override == -2) {
        const char *env_val = getenv("LARECOMP_VK_COPY_SMALL");
        s_override = env_val ? atoll(env_val) : -1;
    }
    return s_override;
}

static inline bool extra_should_copy_draw(
    uint64_t vs_hash,
    uint64_t fs_hash,
    bool depth_test,
    uint32_t count,
    uint32_t instances
) {
    int64_t ovr = extra_copy_small_override();
    if (ovr >= 0) {
        return (count <= (uint64_t)ovr && instances <= AC_UI_MAX_INSTANCES);
    }
    return extra_is_ui_or_dynamic(vs_hash, fs_hash, depth_test, count, instances);
}
