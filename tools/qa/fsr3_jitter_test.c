/* [extra] Self-test for AMD FSR 3.1 Halton(2,3) camera jitter, 3D pass selector,
 * negative MIP LOD bias, and RCAS attenuation math (extra/gpu/fsr3_extra.h). */
#define FSR3_MATH_ONLY
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "hooks/gpu/fsr3_extra.h"

static int near_eq(float a, float b, float eps) {
    return fabsf(a - b) <= eps;
}

int main(void) {
    /* 1. Halton(2,3) bounds and zero-mean over 16 phases */
    float sum_x = 0.0f, sum_y = 0.0f;
    for (uint32_t i = 1; i <= 16; i++) {
        float jx = extra_fsr3_halton(i, 2u);
        float jy = extra_fsr3_halton(i, 3u);
        assert(jx > -0.5f && jx < 0.5f);
        assert(jy > -0.5f && jy < 0.5f);
        sum_x += jx;
        sum_y += jy;
    }
    assert(fabsf(sum_x / 16.0f) < 0.05f);
    assert(fabsf(sum_y / 16.0f) < 0.05f);

    /* 2. Phase count scaling */
    assert(extra_fsr3_phase_count(1920, 1920) == 16u);
    assert(extra_fsr3_phase_count(1280, 1920) == 16u);
    assert(extra_fsr3_phase_count(960, 1920) == 32u);
    assert(extra_fsr3_phase_count(960, 3840) == 64u);

    /* 3. 3D scene pass jitter filter */
    assert(extra_fsr3_should_jitter_pass(1, 1920, 1080, 1, 0) == 1);
    assert(extra_fsr3_should_jitter_pass(1, 1280, 720, 1, 0) == 1);
    assert(extra_fsr3_should_jitter_pass(1, 960, 540, 1, 0) == 1);
    /* Reject shadow maps (1:1 aspect), UI/HUD (no depth), and presented targets */
    assert(extra_fsr3_should_jitter_pass(1, 1024, 1024, 1, 0) == 0);
    assert(extra_fsr3_should_jitter_pass(1, 2048, 2048, 1, 0) == 0);
    assert(extra_fsr3_should_jitter_pass(0, 1920, 1080, 1, 0) == 0);
    assert(extra_fsr3_should_jitter_pass(1, 1920, 1080, 0, 0) == 0);
    assert(extra_fsr3_should_jitter_pass(1, 1920, 1080, 1, 1) == 0);

    /* 4. Negative MIP LOD bias */
    assert(near_eq(extra_fsr3_compute_mip_bias(1.5f, 0), 0.0f, 1e-4f));
    assert(near_eq(extra_fsr3_compute_mip_bias(1.0f, 1), -0.45f, 1e-4f));
    assert(near_eq(extra_fsr3_compute_mip_bias(2.0f, 1), -1.45f, 1e-4f));

    /* 5. RCAS scale attenuation */
    assert(near_eq(extra_fsr3_rcas_scale(0.0f), 0.0f, 1e-4f));
    assert(near_eq(extra_fsr3_rcas_scale(1.0f), 1.0f, 1e-4f));
    assert(extra_fsr3_rcas_scale(0.80f) > extra_fsr3_rcas_scale(0.50f));

    /* 6. Internal 3D scale quantization for exact 1600x900 mip-pyramid dimensions */
    assert(near_eq(extra_fsr3_internal_3d_scale(0.75f), 1.0f, 1e-4f));
    assert(near_eq(extra_fsr3_internal_3d_scale(1.00f), 1.0f, 1e-4f));
    assert(near_eq(extra_fsr3_internal_3d_scale(1.25f), 1.2f, 1e-4f));
    assert(near_eq(extra_fsr3_internal_3d_scale(1.50f), 1.6f, 1e-4f));
    assert(near_eq(extra_fsr3_internal_3d_scale(2.00f), 2.0f, 1e-4f));

    /* 7. Regression guard: non-FSR3 passes must ALWAYS have 0.0f MIP bias at any ratio
     * so non-mipmap / lookup-table samplers never sample uninitialized mip levels. */
    assert(near_eq(extra_fsr3_compute_mip_bias(0.5f, 0), 0.0f, 1e-6f));
    assert(near_eq(extra_fsr3_compute_mip_bias(1.0f, 0), 0.0f, 1e-6f));
    assert(near_eq(extra_fsr3_compute_mip_bias(2.0f, 0), 0.0f, 1e-6f));

    printf("PASSED: FSR 3.1 Halton(2,3) jitter, 3D pass filter, MIP bias, and RCAS math.\n");
    return 0;
}
