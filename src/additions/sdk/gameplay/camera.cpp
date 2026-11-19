#include "camera.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace ac::camera {

static uintptr_t get_slot_camera_manager() {
    static uintptr_t s_slot = 0;
    if (!s_slot) {
        s_slot = ac::memory::resolve_slot_from_aob(ac::memory::AOB_GET_CAMERA_MANAGER, SLOT_CAMERA_MANAGER);
    }
    return s_slot;
}

uintptr_t get_camera_manager() {
    if (!ac::memory::is_guest_memory_accessible()) {
        return 0;
    }
    uintptr_t mgr = 0;
    if (!ac::memory::read<uintptr_t>(get_slot_camera_manager(), &mgr) || !mgr) {
        return 0;
    }
    if ((mgr & 7) != 0 || !ac::memory::is_valid_guest_ptr(mgr, 0x100)) {
        return 0;
    }
    return mgr;
}

uintptr_t get_active_camera() {
    uintptr_t mgr = get_camera_manager();
    if (!mgr) {
        return 0;
    }
    uintptr_t active_cam = 0;
    if (!ac::memory::read_raw<uintptr_t>(mgr + OFFSET_CAM_MGR_ACTIVE, &active_cam) || !active_cam) {
        return 0;
    }
    if ((active_cam & 7) != 0 || !ac::memory::is_valid_guest_ptr(active_cam, 0x1400)) {
        return 0;
    }
    return active_cam;
}

Vec3 get_camera_position() {
    Vec3 pos = {0.0f, 0.0f, 0.0f};
    uintptr_t mgr = get_camera_manager();
    if (!mgr) {
        return pos;
    }
    ac::memory::read_raw<Vec3>(mgr + OFFSET_CAM_MGR_POS, &pos);
    return pos;
}

Vec3 get_camera_target() {
    Vec3 target = {0.0f, 0.0f, 0.0f};
    uintptr_t mgr = get_camera_manager();
    if (!mgr) {
        return target;
    }
    ac::memory::read_raw<Vec3>(mgr + OFFSET_CAM_MGR_TARGET, &target);
    return target;
}

Vec3 get_orientation(uintptr_t camera_ptr) {
    Vec3 result = {0.0f, 0.0f, 0.0f};
    uintptr_t target_cam = camera_ptr ? camera_ptr : get_active_camera();
    if (!target_cam || !ac::memory::is_valid_guest_ptr(target_cam + OFFSET_POS_X, sizeof(Vec3))) {
        return result;
    }
    ac::memory::read_raw<Vec3>(target_cam + OFFSET_POS_X, &result);
    return result;
}

void set_orientation(uintptr_t camera_ptr, const Vec3& ori) {
    uintptr_t target_cam = camera_ptr ? camera_ptr : get_active_camera();
    if (!target_cam || !ac::memory::is_valid_guest_ptr(target_cam + OFFSET_POS_X, sizeof(Vec3))) {
        return;
    }
    void* dst = reinterpret_cast<void*>(target_cam + OFFSET_POS_X);
    std::memcpy(dst, &ori, sizeof(Vec3));
}

float get_pitch(uintptr_t camera_ptr) {
    float pitch = 0.0f;
    uintptr_t target_cam = camera_ptr ? camera_ptr : get_active_camera();
    if (!target_cam || !ac::memory::is_valid_guest_ptr(target_cam + OFFSET_PITCH_ACCUM, sizeof(float))) {
        return pitch;
    }
    ac::memory::read_raw<float>(target_cam + OFFSET_PITCH_ACCUM, &pitch);
    return pitch;
}

void set_pitch(uintptr_t camera_ptr, float pitch) {
    uintptr_t target_cam = camera_ptr ? camera_ptr : get_active_camera();
    if (!target_cam || !ac::memory::is_valid_guest_ptr(target_cam + OFFSET_PITCH_ACCUM, sizeof(float))) {
        return;
    }
    void* dst = reinterpret_cast<void*>(target_cam + OFFSET_PITCH_ACCUM);
    std::memcpy(dst, &pitch, sizeof(float));
}

void inject_mouse_delta(uintptr_t camera_ptr, float delta_x, float delta_y, float sens_x, float sens_y) {
    uintptr_t target_cam = camera_ptr ? camera_ptr : get_active_camera();
    if (!target_cam || !ac::memory::is_valid_guest_ptr(target_cam + OFFSET_INPUT_DELTA_X, sizeof(float) * 2)) {
        return;
    }

    float current_x = 0.0f;
    float current_y = 0.0f;
    ac::memory::read_raw<float>(target_cam + OFFSET_INPUT_DELTA_X, &current_x);
    ac::memory::read_raw<float>(target_cam + OFFSET_INPUT_DELTA_Y, &current_y);

    float new_x = current_x + (delta_x * sens_x);
    float new_y = current_y + (delta_y * sens_y);

    constexpr float PITCH_LIMIT_RAD = PITCH_LIMIT_DEGREES * (3.1415926535f / 180.0f);
    if (new_y > PITCH_LIMIT_RAD) {
        new_y = PITCH_LIMIT_RAD;
    } else if (new_y < -PITCH_LIMIT_RAD) {
        new_y = -PITCH_LIMIT_RAD;
    }

    void* dst_x = reinterpret_cast<void*>(target_cam + OFFSET_INPUT_DELTA_X);
    void* dst_y = reinterpret_cast<void*>(target_cam + OFFSET_INPUT_DELTA_Y);
    std::memcpy(dst_x, &new_x, sizeof(float));
    std::memcpy(dst_y, &new_y, sizeof(float));
}

float get_fov(uintptr_t camera_ptr) {
    uintptr_t target_cam = camera_ptr ? camera_ptr : get_active_camera();
    if (!target_cam || !ac::memory::is_valid_guest_ptr(target_cam + OFFSET_CAMERA_FOV, sizeof(float))) {
        return DEFAULT_FOV_DEGREES;
    }
    float fov = 0.0f;
    if (!ac::memory::read_raw<float>(target_cam + OFFSET_CAMERA_FOV, &fov) || fov < MIN_FOV_DEGREES || fov > MAX_FOV_DEGREES) {
        return DEFAULT_FOV_DEGREES;
    }
    return fov;
}

void set_fov(float degrees, uintptr_t camera_ptr) {
    uintptr_t target_cam = camera_ptr ? camera_ptr : get_active_camera();
    if (!target_cam || !ac::memory::is_valid_guest_ptr(target_cam + OFFSET_CAMERA_FOV, sizeof(float))) {
        return;
    }
    float clamped_fov = std::clamp(degrees, MIN_FOV_DEGREES, MAX_FOV_DEGREES);
    void* dst = reinterpret_cast<void*>(target_cam + OFFSET_CAMERA_FOV);
    std::memcpy(dst, &clamped_fov, sizeof(float));
}

bool is_free_camera_enabled(uintptr_t camera_ptr) {
    uintptr_t target_cam = camera_ptr ? camera_ptr : get_active_camera();
    if (!target_cam || !ac::memory::is_valid_guest_ptr(target_cam + OFFSET_CAMERA_MODE, sizeof(uint32_t))) {
        return false;
    }
    uint32_t mode = CAMERA_MODE_NORMAL;
    if (!ac::memory::read_raw<uint32_t>(target_cam + OFFSET_CAMERA_MODE, &mode)) {
        return false;
    }
    return mode == CAMERA_MODE_FREE;
}

void set_free_camera_enabled(bool enabled, uintptr_t camera_ptr) {
    uintptr_t target_cam = camera_ptr ? camera_ptr : get_active_camera();
    if (!target_cam || !ac::memory::is_valid_guest_ptr(target_cam + OFFSET_CAMERA_MODE, sizeof(uint32_t))) {
        return;
    }
    uint32_t mode = enabled ? CAMERA_MODE_FREE : CAMERA_MODE_NORMAL;
    void* dst = reinterpret_cast<void*>(target_cam + OFFSET_CAMERA_MODE);
    std::memcpy(dst, &mode, sizeof(uint32_t));
}

float get_camera_shake_factor(uintptr_t camera_ptr) {
    uintptr_t target_cam = camera_ptr ? camera_ptr : get_active_camera();
    if (!target_cam || !ac::memory::is_valid_guest_ptr(target_cam + OFFSET_CAMERA_SHAKE, sizeof(float))) {
        return DEFAULT_SHAKE_FACTOR;
    }
    float shake = 0.0f;
    if (!ac::memory::read_raw<float>(target_cam + OFFSET_CAMERA_SHAKE, &shake)) {
        return DEFAULT_SHAKE_FACTOR;
    }
    return shake;
}

void set_camera_shake_factor(float factor, uintptr_t camera_ptr) {
    uintptr_t target_cam = camera_ptr ? camera_ptr : get_active_camera();
    if (!target_cam || !ac::memory::is_valid_guest_ptr(target_cam + OFFSET_CAMERA_SHAKE, sizeof(float))) {
        return;
    }
    void* dst = reinterpret_cast<void*>(target_cam + OFFSET_CAMERA_SHAKE);
    std::memcpy(dst, &factor, sizeof(float));
}

} // namespace ac::camera
