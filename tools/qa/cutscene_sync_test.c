#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define CUTSCENE_FRAME_MS_BASE         (1000.0f / 30.0f)
#define CUTSCENE_DEFAULT_STEP          1.0f
#define CUTSCENE_MAX_CATCHUP_PER_FRAME 7.0f
#define CUTSCENE_MAX_TOTAL_DEBT_FRAMES 150.0f
#define CUTSCENE_MIN_DEBT_LIMIT        -2.0f

typedef struct {
    float debt;
    int was;
    double simulated_video_time_ms;
    double simulated_audio_time_ms;
} CutsceneSimulator;

static void sim_init(CutsceneSimulator *s) {
    s->debt = 0.0f;
    s->was = 0;
    s->simulated_video_time_ms = 0.0;
    s->simulated_audio_time_ms = 0.0;
}

static float sim_step_frame(
    CutsceneSimulator *s,
    float measured_duration_ms,
    float audio_starved_ms
) {
    float measured_frames = measured_duration_ms / CUTSCENE_FRAME_MS_BASE;
    float starved_frames = audio_starved_ms / CUTSCENE_FRAME_MS_BASE;

    s->simulated_audio_time_ms += (double)measured_duration_ms - (double)audio_starved_ms;

    if (!s->was || !(measured_frames > 0.0f) || measured_frames > 9000.0f) {
        measured_frames = CUTSCENE_DEFAULT_STEP;
    }
    s->was = 1;

    s->debt += (measured_frames - CUTSCENE_DEFAULT_STEP) - starved_frames;
    if (s->debt < 0.0f) {
        s->debt = 0.0f;
    }
    if (s->debt > CUTSCENE_MAX_TOTAL_DEBT_FRAMES) {
        s->debt = CUTSCENE_MAX_TOTAL_DEBT_FRAMES;
    }

    float step = CUTSCENE_DEFAULT_STEP;
    if (s->debt >= CUTSCENE_DEFAULT_STEP) {
        float extra = floorf(s->debt);
        if (extra > CUTSCENE_MAX_CATCHUP_PER_FRAME) {
            extra = CUTSCENE_MAX_CATCHUP_PER_FRAME;
        }
        step += extra;
        s->debt -= extra;
    }

    if (s->debt < CUTSCENE_MIN_DEBT_LIMIT) {
        s->debt = CUTSCENE_MIN_DEBT_LIMIT;
    }

    s->simulated_video_time_ms += (double)step * (double)CUTSCENE_FRAME_MS_BASE;
    return step;
}

int main(void) {
    printf("[cutscene_sync_test] Running A/V synchronization & frame pacing regression tests...\n");

    CutsceneSimulator sim;
    sim_init(&sim);

    /* Test 1: Steady 30 FPS playback for 100 frames */
    for (int i = 0; i < 100; i++) {
        float step = sim_step_frame(&sim, CUTSCENE_FRAME_MS_BASE, 0.0f);
        assert(fabsf(step - 1.0f) < 1e-4f);
    }
    assert(fabs(sim.simulated_video_time_ms - sim.simulated_audio_time_ms) < 1.0);
    printf("  [PASS] Steady cadence (100 frames): drift < 1ms\n");

    /* Test 2: Single frame hitch (66.67 ms) */
    float hitch_step = sim_step_frame(&sim, CUTSCENE_FRAME_MS_BASE * 2.0f, 0.0f);
    assert(fabsf(hitch_step - 2.0f) < 1e-4f);
    float next_step = sim_step_frame(&sim, CUTSCENE_FRAME_MS_BASE, 0.0f);
    assert(fabsf(next_step - 1.0f) < 1e-4f);
    assert(fabs(sim.simulated_video_time_ms - sim.simulated_audio_time_ms) < 1.0);
    printf("  [PASS] Single frame hitch: perfectly recovered in 1 frame\n");

    /* Test 3: Major hitch (2.5 seconds = 75 frames) */
    sim_step_frame(&sim, 2500.0f, 0.0f);
    assert(sim.debt > 0.0f);
    /* Catch up across multiple frames */
    for (int i = 0; i < 30; i++) {
        sim_step_frame(&sim, CUTSCENE_FRAME_MS_BASE, 0.0f);
    }
    assert(sim.debt < 1.0f);
    assert(fabs(sim.simulated_video_time_ms - sim.simulated_audio_time_ms) < (double)CUTSCENE_FRAME_MS_BASE);
    printf("  [PASS] 2.5s hitch: catch-up resolved without desync\n");

    /* Test 4: Audio device starvation (300 ms sound stall) */
    /* Video should NOT run ahead of starved audio! */
    sim_init(&sim);
    /* 10 normal frames */
    for (int i = 0; i < 10; i++) {
        sim_step_frame(&sim, CUTSCENE_FRAME_MS_BASE, 0.0f);
    }
    /* 300 ms hitch where audio was also starved by 300 ms */
    sim_step_frame(&sim, 300.0f, 300.0f);
    /* Following frame should NOT rush forward because audio didn't play */
    float post_starve_step = sim_step_frame(&sim, CUTSCENE_FRAME_MS_BASE, 0.0f);
    assert(fabsf(post_starve_step - 1.0f) < 1e-4f);
    assert(fabs(sim.simulated_video_time_ms - sim.simulated_audio_time_ms) <= (double)CUTSCENE_FRAME_MS_BASE + 0.1);
    printf("  [PASS] Audio starvation deduction: video yields to starved sound (drift <= 33.4ms)\n");

    printf("[cutscene_sync_test] All cutscene sync and audio debt tests PASSED!\n");
    return 0;
}
