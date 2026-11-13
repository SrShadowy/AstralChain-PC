// [extra]
#include "../device_internal.h"
#include "additions/overlay/ac_overlay.h" // [extra]
#include "hooks/gpu/fsr3_extra.h" // [extra]

#define VK_IDLE_REPRESENT_DELAY_MS   16u
#define MAX_FT_SAMPLES               1024
#define VK_MS_PER_SEC                1000.0
#define VK_NS_PER_SEC_F              1e9
#define VK_NS_PER_MS_F               1e6
#define VK_NS_PER_MS                 1000000ull
#define VK_ACQUIRE_WARN_THRESHOLD_NS 30000000ull
#define VK_ACQUIRE_WARN_MAX_LOGS     20
#define VK_PRESENT_LOG_LIMIT         5
#define VK_FG_STAGE_REAL             1
#define VK_FG_STAGE_INTERP           2
#define VK_FG_DEFAULT_CYCLE_NS       16666666ull
#define VK_FG_MIN_CYCLE_NS           4000000ull
#define VK_FG_MAX_CYCLE_NS           60000000ull
#define VK_FG_EMA_PREV_WEIGHT        3ull
#define VK_FG_EMA_TOTAL_DIV          4ull
#define VK_FG_MIDPOINT_NUM           42ull
#define VK_FG_MIDPOINT_DEN           100ull
#define VK_DEFAULT_RENDER_WIDTH      1920u
#define VK_SLOW_FRAME_DEFAULT_START  2601L
#define VK_PIPELINE_CACHE_SETTLE_NS  5000000000ull
#define VK_GPUPROF_AUTO_THRESHOLD_NS 250000000ull
#define VK_GPUPROF_AUTO_MAX_COUNT    12u
#define VK_DEFAULT_FPS_LOG_INTERVAL  2.0
#define VK_PCT_P50                   0.50f
#define VK_PCT_P95                   0.95f
#define VK_PCT_P99                   0.99f
#define VK_MIN_VALID_FT_MS           0.001f
#define VK_DIR_MODE_DEFAULT          0755

uint64_t renderer_present_count(void) { return g_dev_state.total_frames; }
uint64_t vk_frame_counter(void) { return g_dev_state.frame_counter; }
const float *vk_shot_crop(void) { return g_dev_state.shot_crop; }

void backend_present_crop(const int crop[4]) {
    memcpy(g_dev_state.present_crop, crop, sizeof g_dev_state.present_crop);
}

void backend_present(NvnTexture *t);
void backend_represent(void) {
    if (g_dev_state.last_presented) backend_present(g_dev_state.last_presented);
    else SDL_Delay(VK_IDLE_REPRESENT_DELAY_MS);
}

static int compare_floats(const void *a, const void *b) {
    float fa = *(const float *)a, fb = *(const float *)b;
    return (fa > fb) - (fa < fb);
}

static float s_ft_samples[MAX_FT_SAMPLES];
static int s_ft_count = 0;
static double s_prev_present, s_max_gap; // [extra]
static void record_present_sample(double now) {
    if (s_prev_present > 0) {
        double delta = now - s_prev_present;
        if (delta > s_max_gap) s_max_gap = delta;
        if (s_ft_count < MAX_FT_SAMPLES) s_ft_samples[s_ft_count++] = (float)(delta * VK_MS_PER_SEC);
    }
    s_prev_present = now;
} // [extra]

static void ensure_debug_frames_dir(void) {
    mkdir("debug", VK_DIR_MODE_DEFAULT);
    mkdir("debug/frames", VK_DIR_MODE_DEFAULT);
}

void backend_present(NvnTexture *t) {
    g_dev_state.last_presented = t;
    ac_overlay_pre_frame(); // [extra]
    TexBackend *b = vk_tex(t, true);
    b->presented = true;
    vk_end_pass();
    static const float black[4] = {0, 0, 0, 1};
    uint64_t t0 = SDL_GetTicksNS();
    bool have = vk_frame_begin(black);
    uint64_t nd = SDL_GetTicksNS() - t0;
    if (nd > VK_ACQUIRE_WARN_THRESHOLD_NS) {
        static int warned;
        if (warned++ < VK_ACQUIRE_WARN_MAX_LOGS) LOG("[vk] acquire blocked %llums\n", (unsigned long long)(nd / VK_NS_PER_MS));
    }
    if (have) {
        // Astral Chain: the game image fills the window at its own aspect ratio (letterboxed), rendered at the
        // game's resolution (render scale 1). No widescreen crop, overlay menu or dynamic resolution.
        double dw = g_vk.sc_extent.width, dh = g_vk.sc_extent.height, gw = t->width ? t->width : dw, gh = t->height ? t->height : dh;
        float uv[4] = {0, 0, 1, 1};
        const int *cr = g_dev_state.present_crop;
        if (t->width && t->height && cr[2] > 0 && cr[3] > 0 && cr[0] >= 0 && cr[1] >= 0 &&
            cr[0] + cr[2] <= t->width && cr[1] + cr[3] <= t->height) {
            uv[0] = (float)cr[0] / t->width; uv[1] = (float)cr[1] / t->height;
            uv[2] = (float)cr[2] / t->width; uv[3] = (float)cr[3] / t->height;
            gw = cr[2]; gh = cr[3];
        }
        memcpy(g_dev_state.shot_crop, uv, sizeof uv);
        double k = dw / gw < dh / gh ? dw / gw : dh / gh, vw = gw * k, vh = gh * k, vx = (dw - vw) / 2, vy = (dh - vh) / 2;
        static int p_log = 0;
        if (p_log++ < VK_PRESENT_LOG_LIMIT) LOG("[present] sc=%ux%u tex=%ux%u crop=[%d,%d,%d,%d] v=(%.1f,%.1f,%.1f,%.1f)\n",
            g_vk.sc_extent.width, g_vk.sc_extent.height, t->width, t->height, cr[0], cr[1], cr[2], cr[3], vx, vy, vw, vh);
        g_extra_fsr3.fg_presenting_interp = extra_fsr3_eff_framegen() ? VK_FG_STAGE_REAL : 0; // [extra]
        VkCommandBuffer cb = vk_cmd();
        float pv[8] = {renderer_origin_lower_left() ? 1.0f : 0.0f, 0, 0, 0, uv[0], uv[1], uv[2], uv[3]};
        vk_present_draw(cb, b, (float)vx, (float)vy, (float)vw, (float)vh, pv);
        ac_overlay_frame(); // [extra]
        static long win_shot = -1;
        if (win_shot < 0) win_shot = getenv("LARECOMP_WINDOW_SHOT") ? atol(getenv("LARECOMP_WINDOW_SHOT")) : 0;
        bool shot = win_shot && (long)g_dev_state.total_frames + 1 == win_shot;
        if (shot) ensure_debug_frames_dir();
        if (g_extra_fsr3.fg_ready) { // [extra]
            static uint64_t s_last_p1_ns;
            static uint64_t s_smoothed_cycle_ns = VK_FG_DEFAULT_CYCLE_NS;
            vk_frame_end(shot ? "debug/frames/window_interp.png" : NULL); // [extra]
            uint64_t now_p1 = SDL_GetTicksNS();
            record_present_sample((double)now_p1 / VK_NS_PER_SEC_F); // [extra]
            if (s_last_p1_ns) {
                uint64_t delta = now_p1 - s_last_p1_ns;
                if (delta >= VK_FG_MIN_CYCLE_NS && delta <= VK_FG_MAX_CYCLE_NS) {
                    s_smoothed_cycle_ns = (s_smoothed_cycle_ns * VK_FG_EMA_PREV_WEIGHT + delta) / VK_FG_EMA_TOTAL_DIV;
                }
            }
            s_last_p1_ns = now_p1;
            uint64_t target_p2_ns = now_p1 + ((s_smoothed_cycle_ns * VK_FG_MIDPOINT_NUM) / VK_FG_MIDPOINT_DEN); // [extra]
            g_dev_state.frames++;
            g_extra_fsr3.fg_presenting_interp = VK_FG_STAGE_INTERP; // [extra]
            extra_fsr3_fg_pace_midpoint(target_p2_ns); // [extra]
            if (vk_frame_begin(black)) {
                VkCommandBuffer cb2 = vk_cmd();
                vk_present_draw(cb2, b, (float)vx, (float)vy, (float)vw, (float)vh, pv);
                ac_overlay_frame();
                vk_frame_end(shot ? "debug/frames/window.png" : NULL);
            } else {
                renderer_submit();
            }
        } else {
            vk_frame_end(shot ? "debug/frames/window.png" : NULL);
        }
        g_extra_fsr3.fg_presenting_interp = 0; // [extra]
        extra_fsr3_frame_step(b->width ? b->width : VK_DEFAULT_RENDER_WIDTH, (uint32_t)lround(vw > 0 ? vw : dw)); // [extra]
    } else {
        renderer_submit();
    }
    extern void vk_prof_frame(uint64_t frame, uint64_t present_ns);
    vk_prof_frame(g_dev_state.total_frames, SDL_GetTicksNS() - t0);
    g_dev_state.frames++;
    g_dev_state.total_frames++;
    g_dev_state.frame_counter++;
    extern void vk_pace_frame(void);
    vk_pace_frame();  // (exec.c: the game's 30 fps mode when frames don't fit 60)
    static long slow_ms = -1;
    if (slow_ms < 0) slow_ms = getenv("LARECOMP_SLOW_FRAME_MS") ? atol(getenv("LARECOMP_SLOW_FRAME_MS")) : 0;
    static long slow_until = -1;  // LARECOMP_SLOW_FRAME_UNTIL=<frame>: the stall ends there (frame pacing recovery tests)
    if (slow_until < 0) slow_until = getenv("LARECOMP_SLOW_FRAME_UNTIL") ? atol(getenv("LARECOMP_SLOW_FRAME_UNTIL")) : 0;
    static long slow_from = -1;  // LARECOMP_SLOW_FRAME_FROM=<frame>: the stall starts there (default 2600)
    if (slow_from < 0) slow_from = getenv("LARECOMP_SLOW_FRAME_FROM") ? atol(getenv("LARECOMP_SLOW_FRAME_FROM")) : VK_SLOW_FRAME_DEFAULT_START;
    static long slow_every = -1;  // LARECOMP_SLOW_FRAME_EVERY=<n>: only every n-th frame of that range
    if (slow_every < 0) slow_every = getenv("LARECOMP_SLOW_FRAME_EVERY") ? atol(getenv("LARECOMP_SLOW_FRAME_EVERY")) : 1;
    if (slow_ms > 0 && (long)g_dev_state.total_frames >= slow_from && (!slow_until || (long)g_dev_state.total_frames < slow_until) &&
        (slow_every <= 1 || ((long)g_dev_state.total_frames - slow_from) % slow_every == 0))
        SDL_Delay((Uint32)slow_ms);
    for (const char *p = getenv("LARECOMP_STALLS"); p && *p;) {  // <frame>:<ms>,...: single stalls (A/V sync tests)
        long at = strtol(p, (char **)&p, 10), ms = *p == ':' ? strtol(p + 1, (char **)&p, 10) : 0;
        if (at == (long)g_dev_state.total_frames && ms > 0) SDL_Delay((Uint32)ms);
        if (*p == ',') p++; else break;
    }
    extern void vk_level_change_check(void);
    vk_level_change_check();
    {   // The pipeline cache was only written at a clean exit, which closing the window or the watchdog never reaches:
        // every session rebuilt every pipeline, and cutscenes stalled behind their audio on first sight. Save it
        // once new builds have settled (5 s without one).
        extern _Atomic unsigned long long vk_pipelines_built;
        static unsigned long long saved_count, seen_count;
        static uint64_t seen_ns;
        unsigned long long built = atomic_load(&vk_pipelines_built);
        uint64_t now_ns = plat_now_ns();
        if (built != seen_count) { seen_count = built; seen_ns = now_ns; }
        else if (built != saved_count && now_ns - seen_ns > VK_PIPELINE_CACHE_SETTLE_NS) { saved_count = built; dev_pipeline_cache_save_async(); }
    }
    {   // textures the game created but has not drawn yet (textures.c)
        extern void vk_texture_warmup(void);
        vk_texture_warmup();
    }
    {   // recorded pipelines of programs that became ready (exec.c)
        extern void vk_pipeline_warmup(void);
        vk_pipeline_warmup();
    }
    static long shot_every = -1;
    if (shot_every < 0) { const char *e = getenv("LARECOMP_SCREENSHOT_EVERY"); shot_every = e ? atol(e) : 0; }
    extern bool g_debug_frame;
    static long debug_at = -1;
    if (debug_at < 0) { const char *e = getenv("LARECOMP_DEBUG_FRAME"); debug_at = e ? atol(e) : 0; }
    extern volatile int g_debug_frame_request;
    if (g_debug_frame_request) { g_debug_frame_request = 0; debug_at = (long)g_dev_state.total_frames + 1; }
    // LARECOMP_DEBUG_FRAMES=<n> (with LARECOMP_DEBUG_FRAME / F9): dump n consecutive frames (debug/frames/pass_<frame>_NNN.png)
    static long debug_n = -1;
    if (debug_n < 0) { const char *e = getenv("LARECOMP_DEBUG_FRAMES"); debug_n = e && atol(e) > 1 ? atol(e) : 1; }
    g_debug_frame = debug_at && g_dev_state.total_frames >= (uint64_t)debug_at && g_dev_state.total_frames < (uint64_t)debug_at + (uint64_t)debug_n;
    {   // LARECOMP_GPUPROF_AT=<frame>[,<frame>...]: GPU time per pass of those frames ("[gpuprof]" in the log)
        extern bool g_gpuprof_frame;
        extern void vk_gpuprof_report(void);
        bool was_profiled = g_gpuprof_frame;
        if (g_gpuprof_frame) { g_gpuprof_frame = false; vk_gpuprof_report(); }
        const char *e = getenv("LARECOMP_GPUPROF_AT");
        for (const char *p = e; p && *p;) {
            long at = strtol(p, (char **)&p, 10);
            if (at == (long)g_dev_state.total_frames) { g_gpuprof_frame = true; break; }
            if (*p == ',') p++; else break;
        }
        static uint64_t prev_ns, auto_profiles;
        static int auto_on = -1;
        if (auto_on < 0) auto_on = getenv("LARECOMP_GPUPROF_AUTO") != NULL;
        uint64_t now_ns = plat_now_ns();
        if (auto_on && !was_profiled && prev_ns && now_ns - prev_ns > VK_GPUPROF_AUTO_THRESHOLD_NS && auto_profiles < VK_GPUPROF_AUTO_MAX_COUNT && !g_gpuprof_frame) {
            auto_profiles++;
            g_gpuprof_frame = true;
            LOG("[gpuprof] frame %llu took %.0f ms: profiling the next frame\n", (unsigned long long)g_dev_state.total_frames, (now_ns - prev_ns) / VK_NS_PER_MS_F);
        }
        prev_ns = now_ns;
    }
    if (g_debug_frame) LOG("===== debug frame %llu =====\n", (unsigned long long)g_dev_state.total_frames);
    {   // LARECOMP_SHOT_AT=<frame>,<frame>,... (ascending): the game image at those frames -> debug/frames/frame_N.png
        static const char *next;
        static int init;
        if (!init) { init = 1; next = getenv("LARECOMP_SHOT_AT"); if (next) ensure_debug_frames_dir(); }
        while (next && *next) {
            long at = strtol(next, (char **)&next, 10);
            if ((long)g_dev_state.total_frames < at) { while (next > getenv("LARECOMP_SHOT_AT") && next[-1] != ',') next--; break; }
            if ((long)g_dev_state.total_frames == at) {
                char path[256];
                snprintf(path, sizeof path, "debug/frames/frame_%05llu.png", (unsigned long long)g_dev_state.total_frames);
                renderer_save_texture_tex(b, path);
            }
            if (*next == ',') next++;
        }
    }
    static long shot_from = -1;
    if (shot_from < 0) { const char *e = getenv("LARECOMP_SCREENSHOT_FROM"); shot_from = e ? atol(e) : 0; }
    if (shot_every && g_dev_state.total_frames % shot_every == 0 && (long)g_dev_state.total_frames >= shot_from) {
        char path[256];
        ensure_debug_frames_dir();
        snprintf(path, sizeof path, "debug/frames/frame_%05llu.png",
                 (unsigned long long)g_dev_state.total_frames);
        if (shot_from) LOG("[shot] frame %llu at %.4f s\n", (unsigned long long)g_dev_state.total_frames, plat_now_ns() / VK_NS_PER_SEC_F);  // (A/V sync)
        renderer_save_texture_tex(b, path);
        // a series (SCREENSHOT_FROM) runs on until LARECOMP_WINDOW_SHOT exits
        if (getenv("LARECOMP_EXIT_AFTER_SHOT") && !shot_from) { fflush(stderr); _exit(0); }
    }
    double now = (double)SDL_GetTicksNS() / VK_NS_PER_SEC_F; // [extra]
    static double fps_iv = -1; // [extra]
    if (fps_iv < 0) fps_iv = getenv("LARECOMP_FPS_INTERVAL") ? atof(getenv("LARECOMP_FPS_INTERVAL")) : VK_DEFAULT_FPS_LOG_INTERVAL;
    record_present_sample(now); // [extra]
    if (now - g_dev_state.last_fps_time >= fps_iv) {
        if (g_dev_state.last_fps_time > 0) {
            float p50 = 0.0f, p95 = 0.0f, p99 = 0.0f, low1 = 0.0f;
            if (s_ft_count > 0) {
                qsort(s_ft_samples, s_ft_count, sizeof(float), compare_floats);
                p50 = s_ft_samples[(int)(s_ft_count * VK_PCT_P50)];
                p95 = s_ft_samples[(int)(s_ft_count * VK_PCT_P95)];
                p99 = s_ft_samples[(int)(s_ft_count * VK_PCT_P99)];
                low1 = (p99 > VK_MIN_VALID_FT_MS) ? ((float)VK_MS_PER_SEC / p99) : 0.0f;
            }
            LOG("[vk] t=%.1f frame %llu %.1f fps [p50=%.1fms p95=%.1fms p99=%.1fms 1%%low=%.1ffps] max %.0fms level %s (%s)\n",
                now, (unsigned long long)g_dev_state.total_frames,
                g_dev_state.frames / (now - g_dev_state.last_fps_time),
                p50, p95, p99, low1,
                s_max_gap * VK_MS_PER_SEC, "-", renderer_stats()); // [extra]
        }
        s_ft_count = 0;
        s_max_gap = 0; // [extra]
        g_dev_state.frames = 0;
        g_dev_state.last_fps_time = now;
    }
}
// [extra]
