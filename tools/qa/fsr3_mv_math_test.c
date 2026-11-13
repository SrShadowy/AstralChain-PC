#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../src/hooks/gpu/velocity_pass.h"

static void make_identity(float m[16]) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = 1.0f;
    m[5] = 1.0f;
    m[10] = 1.0f;
    m[15] = 1.0f;
}

static void make_translation(float m[16], float tx, float ty, float tz) {
    make_identity(m);
    m[12] = tx;
    m[13] = ty;
    m[14] = tz;
}

static void make_rotation_y(float m[16], float rad) {
    make_identity(m);
    float c = cosf(rad);
    float s = sinf(rad);
    m[0] = c;
    m[2] = s;
    m[8] = -s;
    m[10] = c;
}

int main(void) {
    printf("[fsr3_mv_math_test] Testing FSR 3.1 camera motion vector reprojection...\n");

    /* 1. Matrix Inversion Validation */
    float id[16];
    float inv_id[16];
    make_identity(id);
    assert(extra_mat4_invert(id, inv_id) == true);
    for (int i = 0; i < 16; i++) {
        assert(fabsf(inv_id[i] - id[i]) < 1e-6f);
    }
    printf("  [PASS] Identity matrix inversion verified\n");

    /* Translation matrix inverse */
    float tr[16];
    float inv_tr[16];
    make_translation(tr, 10.0f, -5.0f, 2.5f);
    assert(extra_mat4_invert(tr, inv_tr) == true);
    assert(fabsf(inv_tr[12] - (-10.0f)) < 1e-5f);
    assert(fabsf(inv_tr[13] - 5.0f) < 1e-5f);
    assert(fabsf(inv_tr[14] - (-2.5f)) < 1e-5f);
    printf("  [PASS] Translation matrix inversion verified\n");

    /* Singular matrix rejection */
    float singular[16];
    memset(singular, 0, sizeof(singular));
    assert(extra_mat4_invert(singular, inv_tr) == false);
    printf("  [PASS] Degenerate/singular matrix safely rejected\n");

    /* 2. Static Camera (zero velocity) */
    extra_velocity_scene_ubo(id, sizeof(id));
    extra_velocity_end_frame();
    extra_velocity_scene_ubo(id, sizeof(id));
    extra_velocity_compute_camera_hint();

    float cam_x = 1.0f;
    float cam_y = 1.0f;
    extra_velocity_get_camera_mv(&cam_x, &cam_y);
    assert(fabsf(cam_x) < 1e-6f);
    assert(fabsf(cam_y) < 1e-6f);
    printf("  [PASS] Static camera produces exactly zero motion vector\n");

    /* 3. Small camera yaw rotation (0.01 rad horizontal pan) */
    float rot[16];
    make_rotation_y(rot, 0.01f);
    extra_velocity_scene_ubo(id, sizeof(id));
    extra_velocity_end_frame();
    extra_velocity_scene_ubo(rot, sizeof(rot));
    extra_velocity_compute_camera_hint();

    extra_velocity_get_camera_mv(&cam_x, &cam_y);
    /* Camera yaw should produce primarily horizontal delta (cam_x != 0, cam_y ~ 0) */
    assert(fabsf(cam_x) > 1e-4f);
    assert(fabsf(cam_x) <= VELOCITY_MAX_VALID_DELTA);
    assert(fabsf(cam_y) < 1e-4f);
    printf("  [PASS] Horizontal camera yaw produces pure horizontal UV hint: %.5f\n", cam_x);

    printf("[fsr3_mv_math_test] All FSR 3.1 geometric camera velocity tests PASSED!\n");
    return 0;
}
