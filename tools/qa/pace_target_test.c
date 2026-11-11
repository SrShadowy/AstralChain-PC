/* [extra] Pins the frame-pacing heuristic (extra/platform/pace_target.h) to the
 * behaviour it replaced at 60 fps, and to the budget derived from AC_FPS.
 *
 * The 60 fps path is the one every existing build already uses, so it must make
 * the same decision it used to: 12 of the last 60 frames over 16.2 ms enter the
 * game's 30 fps mode, 2 s (120 frames) averaging under 13.0 ms with at most 2
 * over leave it, and the counter floor stays 20 ms. The thresholds are anchored
 * on those three 60 fps values and scaled by the period ratio, so at 60 fps they
 * are the old literals exactly and not a rounding step away.
 *
 * The reference implementation below is the block this replaced in
 * submission.c vk_pace_frame, kept verbatim in behaviour.
 *
 * Build and run: cc -I. extra/tools/pace_target_test.c -lm && ./a.out
 * (audit.sh also runs it with AC_FPS=0, 30 and 144, plus the invalid spellings)
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "extra/platform/pace_target.h"

int g_extra_fps_override = 0;  /* ac_toggles.c */

#define OLD_OVER_MS 16.2
#define OLD_CALM_MS 13.0
#define OLD_SLOW_NS 20000000ull

/* The pre-change heuristic, verbatim in behaviour: the same state and the same
 * per-frame step vk_pace_frame ran, with its own literals. */
typedef struct {
    uint8_t over[60];
    int idx, nover, calm, slow;
    double avg;
} OldPace;

/* Returns 1 on entering the 30 fps mode and 2 on leaving it, like the log lines. */
static int old_step(OldPace *o, double busy_ms) {
    o->avg = o->avg ? o->avg + (busy_ms - o->avg) * 0.05 : busy_ms;
    int over = busy_ms > OLD_OVER_MS;
    o->nover += over - o->over[o->idx];
    o->over[o->idx] = (uint8_t)over;
    o->idx = (o->idx + 1) % 60;
    if (!o->slow && o->nover >= 12) {
        o->slow = 1, o->calm = 0;
        return 1;
    }
    if (o->slow) {
        o->calm = o->avg < OLD_CALM_MS && o->nover <= 2 ? o->calm + 1 : 0;
        if (o->calm >= 120) {
            o->slow = 0, o->nover = 0;
            memset(o->over, 0, sizeof o->over);
            return 2;
        }
    }
    return 0;
}

static int failures;

static void check(int ok, const char *what) {
    if (!ok) { printf("FAIL: %s\n", what); failures++; }
}

/* A deterministic busy-time sequence: calm, a burst that must trip the heuristic,
 * then a calm stretch long enough to leave it. */
static int make_sequence(double *out, int max) {
    int n = 0;
    unsigned seed = 12345;
    for (int i = 0; i < 90 && n < max; i++) out[n++] = 8.0 + (i % 7) * 0.4;
    for (int i = 0; i < 40 && n < max; i++) out[n++] = 18.0 + (i % 5) * 0.9;
    for (int i = 0; i < 300 && n < max; i++) {
        seed = seed * 1103515245u + 12345u;
        out[n++] = 9.0 + (double)((seed >> 16) % 30) / 10.0;  /* 9.0 .. 11.9 ms */
    }
    return n;
}

/* Runs both heuristics over the sequence and compares them frame by frame. The
 * paths may only differ while the fallback is selectable; at 60 fps they must
 * not differ anywhere. */
static void test_matches_old_at_60(void) {
    double busy[512];
    int n = make_sequence(busy, 512);

    ExtraPaceState st;
    OldPace old;
    memset(&st, 0, sizeof st);
    memset(&old, 0, sizeof old);
    int mismatch = 0, enters = 0, leaves = 0, resets = 0;
    for (int i = 0; i < n; i++) {
        int frame_over = 0;
        int ev = extra_pace_update(&st, busy[i], 1, &frame_over);
        enters += ev == EXTRA_PACE_ENTER;
        leaves += ev == EXTRA_PACE_LEAVE;
        resets += ev == EXTRA_PACE_RESET;

        int old_ev = old_step(&old, busy[i]);
        if (frame_over != (busy[i] > OLD_OVER_MS) || st.slow != old.slow ||
                st.nover != old.nover || (ev == EXTRA_PACE_ENTER) != (old_ev == 1) ||
                (ev == EXTRA_PACE_LEAVE) != (old_ev == 2))
            mismatch++;
    }
    check(mismatch == 0, "60 fps: every frame's verdict matches the old heuristic");
    check(enters == 1, "60 fps: the burst enters the 30 fps mode once");
    /* the 300-frame calm tail is below 13.0 ms, so the 120-frame window ends the
     * mode again: one leave, no reset (the fallback is allowed at 60) */
    check(leaves == 1 && resets == 0, "60 fps: the calm tail leaves it once, without a reset");
    check(st.slow == 0, "60 fps: the sequence ends back on the game's own choice");
}

static void test_60_leave(void) {
    ExtraPaceState st;
    memset(&st, 0, sizeof st);
    int over = 0, enters = 0, leaves = 0;
    for (int i = 0; i < 20; i++) enters += extra_pace_update(&st, 20.0, 1, &over) == EXTRA_PACE_ENTER;
    check(enters == 1, "enter once");
    for (int i = 0; i < 400; i++) leaves += extra_pace_update(&st, 8.0, 1, &over) == EXTRA_PACE_LEAVE;
    check(leaves == 1, "leave once after the calm window");
    check(st.slow == 0, "no longer in the 30 fps mode");
}

static void test_60_thresholds_exact(void) {
    check(extra_env_target_fps() == 60, "60 fps is the default target");
    check(extra_pace_budget_ms() == 1000.0 / 60.0, "60 fps budget is one frame");
    check(extra_pace_can_slow() == 1, "60 fps allows the fallback");
    /* The exact literals, not approximately: this is what keeps existing builds
     * making the same decisions. */
    check(extra_pace_over_ms() == OLD_OVER_MS, "over threshold is exactly 16.2 ms");
    check(extra_pace_calm_ms() == OLD_CALM_MS, "calm threshold is exactly 13.0 ms");
    check(extra_pace_slow_ns() == OLD_SLOW_NS, "counter floor is exactly 20 ms");
}

static void test_over_60_never_slows(void) {
    int targets[] = {61, 90, 120, 144, 240};
    for (unsigned i = 0; i < sizeof targets / sizeof targets[0]; i++) {
        g_extra_fps_override = targets[i];
        ExtraPaceState st;
        memset(&st, 0, sizeof st);
        int over = 0, events = 0;
        for (int f = 0; f < 600; f++) events += extra_pace_update(&st, 40.0, 1, &over) != 0;
        check(extra_pace_can_slow() == 0, "above 60 fps the fallback is off");
        check(st.slow == 0 && events == 0, "above 60 fps frames are measured but never trip it");
        check(over == 1, "above 60 fps the over-budget count still runs");
        check(extra_pace_budget_ms() > 0.0, "budget still derives from the target");
        check(extra_pace_slow_ns() > 0, "counter floor still derives from the target");
    }
    g_extra_fps_override = 0;
}

/* AC_FPS=0 (unthrottled): no budget, so no frame is "over" and the fallback is
 * not selectable. Selected by the environment: the overlay override only
 * accepts values > 0. audit.sh runs this file a second time with AC_FPS=0. */
static void test_unthrottled(void) {
    ExtraPaceState st;
    memset(&st, 0, sizeof st);
    int over = 1, events = 0;
    for (int f = 0; f < 600; f++) events += extra_pace_update(&st, 100.0, 1, &over) != 0;
    check(extra_pace_budget_ms() == 0.0, "unthrottled: no budget");
    check(extra_pace_can_slow() == 0, "unthrottled: the fallback is not selectable");
    check(over == 0, "unthrottled: a 100 ms frame is not over budget");
    check(st.slow == 0 && events == 0, "unthrottled: never enters the 30 fps mode");
    check(extra_pace_slow_ns() == 0, "unthrottled: no counter floor");
}

/* Below the reference rate the fallback is still selectable, with thresholds
 * scaled to the longer frame: at 30 fps a 20 ms frame is headroom (32.4 ms is
 * the over threshold) and a 40 ms frame does not fit. */
static void test_scaled_fallback(int target) {
    ExtraPaceState st;
    memset(&st, 0, sizeof st);
    int over = 1, events = 0;
    for (int f = 0; f < 200; f++) events += extra_pace_update(&st, 20.0, 1, &over) != 0;
    check(over == 0, "below the reference rate, a frame under the scaled threshold is fine");
    check(events == 0 && st.slow == 0, "below the reference rate, 20 ms frames never trip it");
    for (int f = 0; f < 40; f++) events += extra_pace_update(&st, 40.0, 1, &over) != 0;
    check(over == 1, "below the reference rate, a 40 ms frame is over the scaled threshold");
    check(st.slow == 1 && events >= 1, "below the reference rate, 40 ms frames do trip it");
    printf("note: target %d fps scaled fallback exercised\n", target);
}

/* The environment lookup keeps the contract of the buffer-based one it
 * replaced: empty or too-long (16 bytes or more) values are unusable and fall
 * through, and 0 means unthrottled. It additionally rejects anything outside
 * [0, EXTRA_TARGET_FPS_MAX] and trailing garbage, so a typo cannot reach the
 * pacer as a rate. audit.sh runs this file under several values of AC_FPS. */
static void test_env_lookup(void) {
    const char *raw = getenv("AC_FPS");
    if (!raw || !*raw) raw = getenv("AC_TARGET_FPS");
    int expected = EXTRA_DEFAULT_TARGET_FPS;
    if (raw && *raw && strlen(raw) < EXTRA_ENV_VALUE_MAX) {
        char *end = NULL;
        long n = strtol(raw, &end, 10);
        if (end != raw && *end == '\0' && n >= 0 && n <= EXTRA_TARGET_FPS_MAX) expected = (int)n;
    }
    check(extra_env_target_fps() == expected, "AC_FPS lookup matches the old buffer rule");
    printf("note: lookup %s -> %d fps\n", raw && *raw ? raw : "(unset)", extra_env_target_fps());
}

int main(void) {
    int target = extra_env_target_fps();
    test_env_lookup();
    if (target == 0) {
        test_unthrottled();
    } else if (target == EXTRA_PACE_REFERENCE_FPS) {
        test_60_thresholds_exact();
        test_matches_old_at_60();
        test_60_leave();
        test_over_60_never_slows();
    } else if (target < EXTRA_PACE_REFERENCE_FPS) {
        check(extra_pace_can_slow() == 1, "below the reference rate the fallback stays selectable");
        check(fabs(extra_pace_over_ms() - OLD_OVER_MS * (60.0 / target)) < 1e-9,
              "over threshold scales with the target");
        check(fabs(extra_pace_calm_ms() - OLD_CALM_MS * (60.0 / target)) < 1e-9,
              "calm threshold scales with the target");
        test_scaled_fallback(target);
    } else {
        /* Above the reference rate: derived, and the fallback is off. audit.sh
         * runs 144 here. */
        check(extra_pace_can_slow() == 0, "above the reference rate the fallback is off");
        check(fabs(extra_pace_over_ms() - OLD_OVER_MS * (60.0 / target)) < 1e-9,
              "over threshold scales with the target");
        check(fabs(extra_pace_calm_ms() - OLD_CALM_MS * (60.0 / target)) < 1e-9,
              "calm threshold scales with the target");
    }
    if (failures) { printf("pace_target: %d check(s) failed\n", failures); return 1; }
    printf("pace_target: OK (target %d fps)\n", target);
    return 0;
}
