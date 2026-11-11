/* SMAA 1x (Jimenez et al., MIT: third_party/smaa/LICENSE.txt) for the ImGui
 * "Antialiasing" option: the reference shader source (SMAA.hlsl built into a C
 * string) and its two precomputed lookup textures, shared by the Vulkan post
 * passes (nvn/vulkan/shaders.c, nvn/vulkan/exec/commands/present.c). */
#include "AreaTex.h"    // 160 x 560, R8G8
#include "SearchTex.h"  // 64 x 16, R8

const char smaa_hlsl_src[] =
#include "smaa_hlsl.inc"
    ;

const unsigned char *smaa_area_tex(void) { return areaTexBytes; }      // AREATEX_WIDTH x AREATEX_HEIGHT, 2 bytes each
const unsigned char *smaa_search_tex(void) { return searchTexBytes; }  // SEARCHTEX_WIDTH x SEARCHTEX_HEIGHT, 1 byte
