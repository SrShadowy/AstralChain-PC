#pragma once
#include <cstdint>
#include "../core/types.hpp"
#include "../core/memory.hpp"

namespace ac::graphics {

// Horizon SVC #0xB3 Hook Sites (Main module offsets for v1.0.1)
constexpr uintptr_t SVC_SITE_DISPLAY_W     = 0x4328c0ull;
constexpr uintptr_t SVC_SITE_DISPLAY_H     = 0x4328c8ull;
constexpr uintptr_t SVC_SITE_STARTUP_W     = 0x40d5e4ull;
constexpr uintptr_t SVC_SITE_STARTUP_H     = 0x40d5f0ull;
constexpr uintptr_t SVC_SITE_RECREATE_W    = 0x40fb04ull;
constexpr uintptr_t SVC_SITE_RECREATE_H    = 0x40fb14ull;
constexpr uintptr_t SVC_SITE_POOL_SIZE     = 0x40e0b0ull;
constexpr uintptr_t SVC_SITE_RENDER_SCALE0 = 0x40f184ull;
constexpr uintptr_t SVC_SITE_RENDER_SCALE1 = 0x40f1a4ull;
constexpr uintptr_t SVC_SITE_RENDER_SCALE2 = 0x40f1d0ull;
constexpr uintptr_t SVC_SITE_BUFROOT       = 0x575a30ull;
constexpr uintptr_t SVC_SITE_GBUF          = 0x575c94ull;
constexpr uintptr_t SVC_SITE_MIP_BIAS      = 0x9ed178ull;
constexpr uintptr_t SVC_SITE_CAMERA_ASPECT = 0x9a30bcull;
constexpr uintptr_t SVC_SITE_CAMERA_RECT   = 0x9a30c4ull;
constexpr uintptr_t SVC_SITE_NOHUD_23      = 0x9177ecull;
constexpr uintptr_t SVC_SITE_NOHUD_22      = 0x915ea0ull;

// Standard aspect ratios
constexpr float ASPECT_16_9  = 16.0f / 9.0f;
constexpr float ASPECT_16_10 = 16.0f / 10.0f;
constexpr float ASPECT_21_9  = 21.0f / 9.0f;
constexpr float ASPECT_32_9  = 32.0f / 9.0f;

// Resolution constraints & alignments
constexpr int32_t RES_GRID_STEP_W    = 32;
constexpr int32_t RES_GRID_STEP_H    = 18;
constexpr int32_t RES_MIN_OUTPUT_W   = 1280;
constexpr int32_t RES_MIN_OUTPUT_H   = 720;
constexpr int32_t RES_MAX_OUTPUT_W   = 3840;
constexpr int32_t RES_MAX_OUTPUT_H   = 2160;
constexpr int32_t RES_DEFAULT_W      = 1600;
constexpr int32_t RES_DEFAULT_H      = 900;

constexpr uint32_t RES_POOL_BASE_BYTES   = 0x13600000u;
constexpr uint64_t RES_POOL_FIXED_BYTES  = (87ull << 20);
constexpr uint64_t RES_POOL_BYTES_PER_PX = 110ull;
constexpr uint64_t RES_1080P_PIXELS      = 1920ull * 1080ull;
constexpr uint64_t RES_GBUF_BYTES_PER_PX = 8ull;
constexpr uint64_t MIB_ALIGN_MASK        = 0xFFFFFull;

struct ResolutionConfig {
    int32_t width        = RES_DEFAULT_W;
    int32_t height       = RES_DEFAULT_H;
    int32_t scene_w      = RES_DEFAULT_W;
    int32_t scene_h      = RES_DEFAULT_H;
    float aspect         = ASPECT_16_9;
    bool is_custom       = false;
    bool aspect_custom   = false;
    uint32_t pool_size   = RES_POOL_BASE_BYTES;
    uint32_t gbuf_extra  = 0;
};

void snap_grid_dimensions(int32_t in_w, int32_t min_w, int32_t max_w, int32_t* out_w, int32_t* out_h);
uint32_t calculate_pool_budget(int32_t w, int32_t h);
uint32_t calculate_gbuffer_extra(int32_t w, int32_t h);
bool is_widescreen(float aspect);

} // namespace ac::graphics
