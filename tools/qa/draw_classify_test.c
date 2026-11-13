#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../src/hooks/gpu/draw_classify.h"

int main(void) {
    printf("[draw_classify_test] Starting UI vs 3D draw classification tests...\n");

    /* 1. UI base shader must always be classified as UI/dynamic */
    assert(extra_is_ui_or_dynamic(AC_UI_BASE_VS_HASH, 0x1234ull, true, 128, 1) == true);
    assert(extra_is_ui_or_dynamic(AC_UI_BASE_VS_HASH, 0x0ull, false, 4, 1) == true);

    /* 2. UI tiling shader (dot patterns behind HUD) must always be classified as UI */
    assert(extra_is_ui_or_dynamic(AC_UI_TILING_VS_HASH, 0x5678ull, true, 256, 1) == true);

    /* 3. Movie/video fragment shaders must be classified as UI/dynamic */
    assert(extra_is_ui_or_dynamic(0x9999ull, AC_VIDEO_FS_HASH_0, true, 6, 1) == true);
    assert(extra_is_ui_or_dynamic(0x9999ull, AC_VIDEO_FS_HASH_1, true, 4, 1) == true);

    /* 4. Small quads (movie presentation quads, backdrops <= 6 vertices) */
    assert(extra_is_ui_or_dynamic(0x1111ull, 0x2222ull, true, 4, 1) == true);
    assert(extra_is_ui_or_dynamic(0x1111ull, 0x2222ull, false, 6, 1) == true);

    /* 5. Non-depth-tested UI overlays (<= 64 vertices, <= 4 instances) */
    assert(extra_is_ui_or_dynamic(0x1111ull, 0x2222ull, false, 32, 2) == true);
    assert(extra_is_ui_or_dynamic(0x1111ull, 0x2222ull, false, 64, 4) == true);

    /* 6. 3D Meshes with depth test must NEVER be classified as UI/dynamic */
    /* Small 3D prop/debris (120 vertices, depth test on) */
    assert(extra_is_ui_or_dynamic(0x3333ull, 0x4444ull, true, 120, 1) == false);

    /* Standard character mesh (1500 vertices, depth test on) */
    assert(extra_is_ui_or_dynamic(0x3333ull, 0x4444ull, true, 1500, 1) == false);

    /* Large environment mesh (10000 vertices, depth test on) */
    assert(extra_is_ui_or_dynamic(0x3333ull, 0x4444ull, true, 10000, 1) == false);

    /* Instanced foliage/props (8 instances, depth test on) */
    assert(extra_is_ui_or_dynamic(0x3333ull, 0x4444ull, true, 64, 8) == false);

    /* 7. Default extra_should_copy_draw (no env override) */
    unsetenv("LARECOMP_VK_COPY_SMALL");
    assert(extra_should_copy_draw(AC_UI_BASE_VS_HASH, 0, false, 128, 1) == true);
    assert(extra_should_copy_draw(0x3333ull, 0x4444ull, true, 120, 1) == false);

    printf("[draw_classify_test] All classification tests passed successfully!\n");
    return 0;
}
