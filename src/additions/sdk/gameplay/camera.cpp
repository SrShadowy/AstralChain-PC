#include "camera.hpp"
#include <cmath>
#include <cstring>

namespace ac::camera {

uintptr_t get_camera_manager() {
    uintptr_t mgr = 0;
    if (!ac::memory::read<uintptr_t>(SLOT_CAMERA_MANAGER, &mgr) || !mgr) {
        return ac::memory::resolve<uintptr_t>(CAMERA_MANAGER_DIRECT) ? (ac::memory::get_main_base() + CAMERA_MANAGER_DIRECT) : 0;
    }
    return mgr;
}

uintptr_t get_active_camera() {
    uintptr_t mgr = get_camera_manager();
    if (!mgr) {
        return 0;
    }
    uintptr_t active_cam = 0;
    const void* src = reinterpret_cast<const void*>(mgr + OFFSET_CAM_MGR_ACTIVE);
    std::memcpy(&active_cam, src, sizeof(uintptr_t));
    return active_cam;
}

Vec3 get_camera_position() {
    Vec3 pos = {0.0f, 0.0f, 0.0f};
    uintptr_t mgr = get_camera_manager();
    if (!mgr) {
        return pos;
    }
    const void* src = reinterpret_cast<const void*>(mgr + OFFSET_CAM_MGR_POS);
    std::memcpy(&pos, src, sizeof(Vec3));
    return pos;
}

Vec3 get_camera_target() {
    Vec3 target = {0.0f, 0.0f, 0.0f};
    uintptr_t mgr = get_camera_manager();
    if (!mgr) {
        return target;
    }
    const void* src = reinterpret_cast<const void*>(mgr + OFFSET_CAM_MGR_TARGET);
    std::memcpy(&target, src, sizeof(Vec3));
    return target;
}

Vec3 get_orientation(uintptr_t camera_ptr) {
    Vec3 result = {0.0f, 0.0f, 0.0f};
    if (!camera_ptr) {
        return result;
    }
    const void* src = reinterpret_cast<const void*>(camera_ptr + OFFSET_POS_X);
    std::memcpy(&result, src, sizeof(Vec3));
    return result;
}

void set_orientation(uintptr_t camera_ptr, const Vec3& ori) {
    if (!camera_ptr) {
        return;
    }
    void* dst = reinterpret_cast<void*>(camera_ptr + OFFSET_POS_X);
    std::memcpy(dst, &ori, sizeof(Vec3));
}

float get_pitch(uintptr_t camera_ptr) {
    float pitch = 0.0f;
    if (!camera_ptr) {
        return pitch;
    }
    const void* src = reinterpret_cast<const void*>(camera_ptr + OFFSET_PITCH_ACCUM);
    std::memcpy(&pitch, src, sizeof(float));
    return pitch;
}

void set_pitch(uintptr_t camera_ptr, float pitch) {
    if (!camera_ptr) {
        return;
    }
    void* dst = reinterpret_cast<void*>(camera_ptr + OFFSET_PITCH_ACCUM);
    std::memcpy(dst, &pitch, sizeof(float));
}

void inject_mouse_delta(uintptr_t camera_ptr, float delta_x, float delta_y, float sens_x, float sens_y) {
    if (!camera_ptr) {
        return;
    }

    float current_x = 0.0f;
    float current_y = 0.0f;
    const void* src_x = reinterpret_cast<const void*>(camera_ptr + OFFSET_INPUT_DELTA_X);
    const void* src_y = reinterpret_cast<const void*>(camera_ptr + OFFSET_INPUT_DELTA_Y);
    std::memcpy(&current_x, src_x, sizeof(float));
    std::memcpy(&current_y, src_y, sizeof(float));

    float new_x = current_x + (delta_x * sens_x);
    float new_y = current_y + (delta_y * sens_y);

    constexpr float PITCH_LIMIT_RAD = PITCH_LIMIT_DEGREES * (3.1415926535f / 180.0f);
    if (new_y > PITCH_LIMIT_RAD) {
        new_y = PITCH_LIMIT_RAD;
    } else if (new_y < -PITCH_LIMIT_RAD) {
        new_y = -PITCH_LIMIT_RAD;
    }

    void* dst_x = reinterpret_cast<void*>(camera_ptr + OFFSET_INPUT_DELTA_X);
    void* dst_y = reinterpret_cast<void*>(camera_ptr + OFFSET_INPUT_DELTA_Y);
    std::memcpy(dst_x, &new_x, sizeof(float));
    std::memcpy(dst_y, &new_y, sizeof(float));
}

} // namespace ac::camera
