#pragma once
#include <cstdint>
#include "../core/types.hpp"
#include "../core/memory.hpp"

namespace ac::camera {

// Camera vtable & entrypoint routines
constexpr uintptr_t VTABLE_OFFSET          = 0xd5b3d0ull;
constexpr uintptr_t VTABLE_RTTI_OFFSET     = 0xd5b3c0ull;
constexpr uintptr_t METHOD_UPDATE_CAMERA   = 0x27f640ull;
constexpr uintptr_t CAMERA_DIRECTOR_UPDATE = 0x28b120ull;
constexpr uintptr_t FREE_CAMERA_UPDATE     = 0x2bd8b0ull;

// Camera Manager Singleton Slots & Storage
constexpr uintptr_t SLOT_CAMERA_MANAGER    = 0xdb83a0ull;
constexpr uintptr_t CAMERA_MANAGER_DIRECT  = 0xdca490ull;
constexpr uintptr_t GET_CAMERA_MANAGER_FN  = 0x88fa20ull;

// Offsets inside CameraManager
constexpr uintptr_t OFFSET_CAM_MGR_POS     = 0x10ull;
constexpr uintptr_t OFFSET_CAM_MGR_TARGET  = 0x20ull;
constexpr uintptr_t OFFSET_CAM_MGR_UP      = 0x30ull;
constexpr uintptr_t OFFSET_CAM_MGR_RIGHT   = 0x40ull;
constexpr uintptr_t OFFSET_CAM_MGR_ACTIVE  = 0x58ull;

// Offsets inside active Camera Controller instance
constexpr uintptr_t OFFSET_CAMERA_FOV      = 0x60ull;
constexpr uintptr_t OFFSET_CAMERA_MODE     = 0x644ull;
constexpr uintptr_t OFFSET_CAMERA_SHAKE    = 0x658ull;
constexpr uintptr_t OFFSET_INPUT_DELTA_X   = 0x8d8ull;
constexpr uintptr_t OFFSET_INPUT_DELTA_Y   = 0x8e0ull;
constexpr uintptr_t OFFSET_POS_X           = 0xb50ull;
constexpr uintptr_t OFFSET_POS_Y           = 0xb54ull;
constexpr uintptr_t OFFSET_POS_Z           = 0xb58ull;
constexpr uintptr_t OFFSET_PITCH_ACCUM     = 0x1284ull;
constexpr uintptr_t OFFSET_RIGHT_STICK_IN  = 0x12e0ull;

// Camera Limits & Configuration Constants
constexpr float PITCH_LIMIT_DEGREES     = 90.0f;
constexpr float DEFAULT_FOV_DEGREES     = 45.0f;
constexpr float MIN_FOV_DEGREES         = 10.0f;
constexpr float MAX_FOV_DEGREES         = 120.0f;
constexpr float DEFAULT_SHAKE_FACTOR    = 1.0f;

constexpr uint32_t CAMERA_MODE_NORMAL   = 0u;
constexpr uint32_t CAMERA_MODE_FREE     = 1u;

uintptr_t get_camera_manager();
uintptr_t get_active_camera();
Vec3 get_camera_position();
Vec3 get_camera_target();

Vec3 get_orientation(uintptr_t camera_ptr = 0);
void set_orientation(uintptr_t camera_ptr, const Vec3& ori);
float get_pitch(uintptr_t camera_ptr = 0);
void set_pitch(uintptr_t camera_ptr, float pitch);
void inject_mouse_delta(uintptr_t camera_ptr, float delta_x, float delta_y, float sens_x, float sens_y);

// FOV & Projection Control
float get_fov(uintptr_t camera_ptr = 0);
void set_fov(float degrees, uintptr_t camera_ptr = 0);

// FreeCamera / Photo Mode Toggle
bool is_free_camera_enabled(uintptr_t camera_ptr = 0);
void set_free_camera_enabled(bool enabled, uintptr_t camera_ptr = 0);

// Camera Shake Modifier
float get_camera_shake_factor(uintptr_t camera_ptr = 0);
void set_camera_shake_factor(float factor, uintptr_t camera_ptr = 0);

} // namespace ac::camera
