#pragma once
/* [extra] The presentation target, shared by the two places that need it: the NVN
 * pacer (extra/platform/pace_trampoline.h) and the frame-pacing heuristic
 * (astral_chain/runtime/las/nvn/vulkan/exec/submission/submission.c).
 *
 * AC_FPS is the rate the port paces presents to. It is not the rate the game's
 * own timer runs at: the Dynamic-FPS patch replaces that with a step measured
 * from real frame time, so a target above 60 changes how often a frame is shown,
 * not how fast the world advances. */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "additions/toggles/ac_toggles.h"

/* Pacing targets, in frames per second. */
#define EXTRA_DEFAULT_TARGET_FPS 60
#define EXTRA_CUTSCENE_FPS       30  /* svc #0xB1 in host_runtime.c.in: cutscenes present at 30 */

/* The rate the game's own fixed step and its 30/60 mode switch are built for. The
 * fallback below that rate is only meaningful up to here: above it the measured
 * step keeps the world's speed correct at any refresh rate, so halving the rate
 * would only throw away smoothness. */
#define EXTRA_PACE_REFERENCE_FPS 60

/* The three millisecond values this check hardcoded before it was derived: the
 * busy time that counts as "over budget", the headroom average, and the counter
 * floor reported while in the game's 30 fps mode. They are thresholds at
 * EXTRA_PACE_REFERENCE_FPS and are scaled by the period ratio, so 60 fps stays
 * bit-for-bit what it was instead of picking up a rounding step from
 * (1000/60) x fraction. */
#define EXTRA_PACE_OVER_MS 16.2
#define EXTRA_PACE_CALM_MS 13.0
#define EXTRA_PACE_SLOW_MS 20.0

/* Sentinels for the one-shot environment lookups. UNREAD means "not cached
 * yet"; UNSET means "variable absent or unusable", meaning the caller keeps the
 * game's own value. They must stay distinct so a real value of 0 (unthrottled)
 * can be told apart from "no override configured". */
#define EXTRA_ENV_UNREAD (-2)
#define EXTRA_ENV_UNSET  (-1)

/* A value that fills the whole buffer may have been truncated, so it is
 * rejected and the default is used instead. */
#define EXTRA_ENV_VALUE_MAX 16

/* Accepted target range. 0 is the unthrottled sentinel; the upper bound keeps
 * the pacer's refresh period (RT_TICKS_PER_SECOND / fps) at one tick or more
 * and rejects a number large enough to be a typo rather than a target. */
#define EXTRA_TARGET_FPS_MAX 1000

/* Reads a non-empty environment value that is short enough to be trusted. An
 * unusable value (too long) is treated as absent, matching the buffer-based
 * lookup this replaced. */
static inline const char *extra_env_value(const char *name) {
    const char *v = getenv(name);
    return (v && *v && strlen(v) < EXTRA_ENV_VALUE_MAX) ? v : NULL;
}

/* A complete decimal number in [0, EXTRA_TARGET_FPS_MAX], or EXTRA_ENV_UNSET.
 * strtol rather than atoi: a value that does not fit an int is unusable, not
 * undefined, and trailing garbage ("60fps") is rejected instead of read as 60. */
static inline int extra_env_fps(const char *name) {
    const char *v = extra_env_value(name);
    if (!v) return EXTRA_ENV_UNSET;
    char *end = NULL;
    long n = strtol(v, &end, 10);
    if (end == v || *end != '\0' || n < 0 || n > EXTRA_TARGET_FPS_MAX) return EXTRA_ENV_UNSET;
    return (int)n;
}

/* AC_FPS, falling back to the legacy AC_TARGET_FPS spelling, then to 60.
 * 0 means unthrottled. The overlay (g_extra_fps_override) wins while set. */
static inline int extra_env_target_fps(void) {
    if (g_extra_fps_override >= 0) return g_extra_fps_override; /* [extra] overlay/INI override */
    static int cached = EXTRA_ENV_UNREAD;
    if (cached == EXTRA_ENV_UNREAD) {
        cached = extra_env_fps("AC_FPS");
        if (cached == EXTRA_ENV_UNSET) cached = extra_env_fps("AC_TARGET_FPS");
        if (cached == EXTRA_ENV_UNSET) cached = EXTRA_DEFAULT_TARGET_FPS;
    }
    return cached;
}

/* Frame period, in ms, at the target rate. 0 when unthrottled (no budget). */
static inline double extra_pace_budget_ms(void) {
    int fps = extra_env_target_fps();
    return fps > 0 ? 1000.0 / (double)fps : 0.0;
}

/* How much longer a frame period is at the target rate than at the reference
 * rate. The thresholds below are constants at the reference rate; this is the
 * factor that carries them to the caller's target. 0 when unthrottled (there is
 * no target rate to scale to). */
static inline double extra_pace_scale(void) {
    int fps = extra_env_target_fps();
    return fps > 0 ? (double)EXTRA_PACE_REFERENCE_FPS / (double)fps : 0.0;
}

/* Busy time above which a frame does not fit the target. 0 when unthrottled:
 * with no target rate there is no budget to exceed. */
static inline double extra_pace_over_ms(void) {
    return EXTRA_PACE_OVER_MS * extra_pace_scale();
}

/* Average busy time, at or under which a frame counts as headroom. */
static inline double extra_pace_calm_ms(void) {
    return EXTRA_PACE_CALM_MS * extra_pace_scale();
}

/* Whether the port may fall back to the game's own 30 fps mode when frames do not
 * fit. Only at or below the reference rate: above it the measured step already
 * keeps the game's speed correct, so the mode has nothing to protect. */
static inline int extra_pace_can_slow(void) {
    int fps = extra_env_target_fps();
    return fps > 0 && fps <= EXTRA_PACE_REFERENCE_FPS;
}

/* The counter time reported while in the game's 30 fps mode: 20 ms at 60 fps,
 * scaled with the target, so the game sees a frame that does not fit its fast
 * mode. 0 when unthrottled (the fallback is not selectable there). */
static inline uint64_t extra_pace_slow_ns(void) {
    return (uint64_t)(EXTRA_PACE_SLOW_MS * extra_pace_scale() * 1e6);
}

/* ---- the frame-pacing heuristic (vk_pace_frame in submission.c) ----
 *
 * Each presented frame's busy time is the period between presents minus the time
 * the presenting thread spent blocked in kernel waits and sleeps (idle). The game
 * picks its 30 fps mode from GPU counters; our frames can be slow for reasons
 * those do not show, so the port reports a frame of at least extra_pace_slow_ns()
 * while this heuristic says frames do not fit.
 *
 * Both windows are kept in frames, as before: enter when 12 of the last 60 frames
 * are over budget, leave after 2 s (120 frames) of headroom with at most 2 over.
 * The averages differ from a pure count because single spikes are normal play. */
typedef struct {
    uint8_t over[60];
    int idx, nover, calm, slow;
    double avg;  /* busy ms, moving average (~20 frames) */
} ExtraPaceState;

#define EXTRA_PACE_NO_EVENT 0
#define EXTRA_PACE_ENTER    1
#define EXTRA_PACE_LEAVE    2  /* the 30 fps mode went back to the game's own choice (headroom) */
#define EXTRA_PACE_RESET    3  /* the fallback was disabled or the target no longer allows it */

/* Feeds one presented frame. Always measures (the caller reports the count even
 * with the fallback disabled); only selects the 30 fps mode when `enabled` and
 * the target is at most the reference rate. `over` receives this frame's verdict. */
static inline int extra_pace_update(ExtraPaceState *st, double busy_ms, int enabled, int *over) {
    double over_ms = extra_pace_over_ms();
    *over = over_ms > 0.0 && busy_ms > over_ms;
    st->avg = st->avg ? st->avg + (busy_ms - st->avg) * 0.05 : busy_ms;
    st->nover += *over - st->over[st->idx];
    st->over[st->idx] = (uint8_t)*over;
    st->idx = (st->idx + 1) % 60;
    if (!enabled || !extra_pace_can_slow()) {
        int was_slow = st->slow;
        st->slow = 0, st->nover = 0, st->calm = 0;
        memset(st->over, 0, sizeof st->over);
        return was_slow ? EXTRA_PACE_RESET : EXTRA_PACE_NO_EVENT;
    }
    if (!st->slow && st->nover >= 12) {
        st->slow = 1, st->calm = 0;
        return EXTRA_PACE_ENTER;
    }
    if (st->slow) {
        st->calm = st->avg < extra_pace_calm_ms() && st->nover <= 2 ? st->calm + 1 : 0;
        if (st->calm >= 120) {
            st->slow = 0, st->nover = 0;
            memset(st->over, 0, sizeof st->over);
            return EXTRA_PACE_LEAVE;
        }
    }
    return EXTRA_PACE_NO_EVENT;
}
