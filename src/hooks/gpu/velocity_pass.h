#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define VELOCITY_MATRIX_SIZE       16
#define VELOCITY_DEFAULT_DEPTH     0.95f
#define VELOCITY_MAX_VALID_DELTA   0.5f

typedef struct {
    float vp_curr[VELOCITY_MATRIX_SIZE];
    float vp_prev[VELOCITY_MATRIX_SIZE];
    bool has_curr;
    bool has_prev;
    float cam_mv_x;
    float cam_mv_y;
} VelocityTracker;

static VelocityTracker g_velocity_state = {
    {0},
    {0},
    false,
    false,
    0.0f,
    0.0f
};

/* Invert a 4x4 matrix. Returns false if determinant is near zero. */
static inline bool extra_mat4_invert(const float m[16], float inv[16]) {
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
             m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] -
             m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
             m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] -
              m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];

    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (fabsf(det) < 1e-8f) {
        return false;
    }

    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] -
             m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
             m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
             m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
              m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];

    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] +
             m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
             m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
              m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
               m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
             m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
             m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
              m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
              m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    float inv_det = 1.0f / det;
    for (int i = 0; i < 16; i++) {
        inv[i] *= inv_det;
    }
    return true;
}

static inline void extra_mat4_transform_vec4(
    const float m[16],
    const float in[4],
    float out[4]
) {
    out[0] = m[0] * in[0] + m[4] * in[1] + m[8] * in[2] + m[12] * in[3];
    out[1] = m[1] * in[0] + m[5] * in[1] + m[9] * in[2] + m[13] * in[3];
    out[2] = m[2] * in[0] + m[6] * in[1] + m[10] * in[2] + m[14] * in[3];
    out[3] = m[3] * in[0] + m[7] * in[1] + m[11] * in[2] + m[15] * in[3];
}

/* Records camera view-projection matrix from scene UBO binding 5 */
static inline void extra_velocity_scene_ubo(const float *u, uint64_t size) {
    if (!u || size < (VELOCITY_MATRIX_SIZE * sizeof(float))) {
        return;
    }
    memcpy(g_velocity_state.vp_curr, u, VELOCITY_MATRIX_SIZE * sizeof(float));
    g_velocity_state.has_curr = true;
}

/* Calculates camera velocity vector in UV space based on frame reprojection */
static inline void extra_velocity_compute_camera_hint(void) {
    if (!g_velocity_state.has_curr || !g_velocity_state.has_prev) {
        g_velocity_state.cam_mv_x = 0.0f;
        g_velocity_state.cam_mv_y = 0.0f;
        return;
    }

    float inv_curr[16];
    if (!extra_mat4_invert(g_velocity_state.vp_curr, inv_curr)) {
        g_velocity_state.cam_mv_x = 0.0f;
        g_velocity_state.cam_mv_y = 0.0f;
        return;
    }

    /* Screen center point in normalized device coordinates */
    float p_ndc[4] = {0.0f, 0.0f, VELOCITY_DEFAULT_DEPTH, 1.0f};
    float p_world[4];
    extra_mat4_transform_vec4(inv_curr, p_ndc, p_world);
    if (fabsf(p_world[3]) < 1e-6f) {
        return;
    }
    p_world[0] /= p_world[3];
    p_world[1] /= p_world[3];
    p_world[2] /= p_world[3];
    p_world[3] = 1.0f;

    /* Reproject into previous camera clip space */
    float p_prev_clip[4];
    extra_mat4_transform_vec4(g_velocity_state.vp_prev, p_world, p_prev_clip);
    if (fabsf(p_prev_clip[3]) < 1e-6f) {
        return;
    }

    float prev_ndc_x = p_prev_clip[0] / p_prev_clip[3];
    float prev_ndc_y = p_prev_clip[1] / p_prev_clip[3];

    /* Convert NDC delta to UV motion delta:
     * uv = (ndc * 0.5) + 0.5
     * delta_uv = (ndc_curr - ndc_prev) * 0.5 */
    float mv_x = (0.0f - prev_ndc_x) * 0.5f;
    float mv_y = (0.0f - prev_ndc_y) * -0.5f;

    if (fabsf(mv_x) <= VELOCITY_MAX_VALID_DELTA && fabsf(mv_y) <= VELOCITY_MAX_VALID_DELTA) {
        g_velocity_state.cam_mv_x = mv_x;
        g_velocity_state.cam_mv_y = mv_y;
    } else {
        g_velocity_state.cam_mv_x = 0.0f;
        g_velocity_state.cam_mv_y = 0.0f;
    }
}

static inline void extra_velocity_get_camera_mv(float *out_x, float *out_y) {
    if (out_x) {
        *out_x = g_velocity_state.cam_mv_x;
    }
    if (out_y) {
        *out_y = g_velocity_state.cam_mv_y;
    }
}

static inline void extra_velocity_end_frame(void) {
    if (g_velocity_state.has_curr) {
        memcpy(g_velocity_state.vp_prev, g_velocity_state.vp_curr, sizeof(g_velocity_state.vp_prev));
        g_velocity_state.has_prev = true;
    }
    extra_velocity_compute_camera_hint();
}
