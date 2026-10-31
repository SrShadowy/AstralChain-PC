// [extra]
#include "../device_internal.h"

uint64_t renderer_present_count(void) { return g_dev_state.total_frames; }
uint64_t vk_frame_counter(void) { return g_dev_state.frame_counter; }
const float *vk_shot_crop(void) { return g_dev_state.shot_crop; }

void backend_present_crop(const int crop[4]) {
    memcpy(g_dev_state.present_crop, crop, sizeof g_dev_state.present_crop);
}

void backend_present(NvnTexture *t);
void backend_represent(void) {
    if (g_dev_state.last_presented) backend_present(g_dev_state.last_presented);
    else SDL_Delay(16);
}

static int compare_floats(const void *a, const void *b) {
    float fa = *(const float *)a, fb = *(const float *)b;
    return (fa > fb) - (fa < fb);
}

#define MAX_FT_SAMPLES 512
static float s_ft_samples[MAX_FT_SAMPLES];
static int s_ft_count = 0;

void backend_present(NvnTexture *t) {
    g_dev_state.last_presented = t;
    TexBackend *b = vk_tex(t, true);
    b->presented = true;
    vk_end_pass();
    static const float black[4] = {0, 0, 0, 1};
    uint64_t t0 = SDL_GetTicksNS();
    bool have = vk_frame_begin(black);
    uint64_t nd = SDL_GetTicksNS() - t0;
    if (nd > 30000000) {
        static int warned;
        if (warned++ < 20) LOG("[vk] acquire blocked %llums\n", (unsigned long long)(nd / 1000000));
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
        if (p_log++ < 5) LOG("[present] sc=%ux%u tex=%ux%u crop=[%d,%d,%d,%d] v=(%.1f,%.1f,%.1f,%.1f)\n",
            g_vk.sc_extent.width, g_vk.sc_extent.height, t->width, t->height, cr[0], cr[1], cr[2], cr[3], vx, vy, vw, vh);
        renderer_set_scale(1.0f, 1.0f);
        VkCommandBuffer cb = vk_cmd();
        float pv[8] = {renderer_origin_lower_left() ? 1.0f : 0.0f, 0, 0, 0, uv[0], uv[1], uv[2], uv[3]};
        vk_present_draw(cb, b, (float)vx, (float)vy, (float)vw, (float)vh, pv);
        static long win_shot = -1;
        if (win_shot < 0) win_shot = getenv("LARECOMP_WINDOW_SHOT") ? atol(getenv("LARECOMP_WINDOW_SHOT")) : 0;
        bool shot = win_shot && (long)g_dev_state.total_frames + 1 == win_shot;
        vk_frame_end(shot ? "window.png" : NULL);
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
    if (slow_from < 0) slow_from = getenv("LARECOMP_SLOW_FRAME_FROM") ? atol(getenv("LARECOMP_SLOW_FRAME_FROM")) : 2601;
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
        else if (built != saved_count && now_ns - seen_ns > 5000000000ull) { saved_count = built; dev_pipeline_cache_save_async(); }
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
    // LARECOMP_DEBUG_FRAMES=<n> (with LARECOMP_DEBUG_FRAME / F9): dump n consecutive frames (build/pass_<frame>_NNN.png)
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
        if (auto_on && !was_profiled && prev_ns && now_ns - prev_ns > 250000000ull && auto_profiles < 12 && !g_gpuprof_frame) {
            auto_profiles++;
            g_gpuprof_frame = true;
            LOG("[gpuprof] frame %llu took %.0f ms: profiling the next frame\n", (unsigned long long)g_dev_state.total_frames, (now_ns - prev_ns) / 1e6);
        }
        prev_ns = now_ns;
    }
    if (g_debug_frame) LOG("===== debug frame %llu =====\n", (unsigned long long)g_dev_state.total_frames);
    {   // LARECOMP_SHOT_AT=<frame>,<frame>,... (ascending): the game image at those frames -> build/frames/frame_N.png
        static const char *next;
        static int init;
        if (!init) { init = 1; next = getenv("LARECOMP_SHOT_AT"); if (next) mkdir("frames", 0755); }
        while (next && *next) {
            long at = strtol(next, (char **)&next, 10);
            if ((long)g_dev_state.total_frames < at) { while (next > getenv("LARECOMP_SHOT_AT") && next[-1] != ',') next--; break; }
            if ((long)g_dev_state.total_frames == at) {
                char path[256];
                snprintf(path, sizeof path, "frames/frame_%05llu.png", (unsigned long long)g_dev_state.total_frames);
                renderer_save_texture_tex(b, path);
            }
            if (*next == ',') next++;
        }
    }
    static long shot_from = -1;
    if (shot_from < 0) { const char *e = getenv("LARECOMP_SCREENSHOT_FROM"); shot_from = e ? atol(e) : 0; }
    if (shot_every && g_dev_state.total_frames % shot_every == 0 && (long)g_dev_state.total_frames >= shot_from) {
        char path[256];
        if (shot_from) mkdir("frames", 0755);
        snprintf(path, sizeof path, shot_from ? "frames/frame_%05llu.png" : "frame_%05llu.png",
                 (unsigned long long)g_dev_state.total_frames);
        if (shot_from) LOG("[shot] frame %llu at %.4f s\n", (unsigned long long)g_dev_state.total_frames, plat_now_ns() / 1e9);  // (A/V sync)
        renderer_save_texture_tex(b, path);
        // a series (SCREENSHOT_FROM) runs on until LARECOMP_WINDOW_SHOT exits
        if (getenv("LARECOMP_EXIT_AFTER_SHOT") && !shot_from) { fflush(stderr); _exit(0); }
    }
    double now = SDL_GetTicks() / 1000.0;
    static double fps_iv = -1, prev_present, max_gap;
    if (fps_iv < 0) fps_iv = getenv("LARECOMP_FPS_INTERVAL") ? atof(getenv("LARECOMP_FPS_INTERVAL")) : 2.0;
    if (prev_present > 0) {
        double delta = now - prev_present;
        if (delta > max_gap) max_gap = delta;
        if (s_ft_count < MAX_FT_SAMPLES) {
            s_ft_samples[s_ft_count++] = (float)(delta * 1000.0);
        }
    }
    prev_present = now;
    if (now - g_dev_state.last_fps_time >= fps_iv) {
        if (g_dev_state.last_fps_time > 0) {
            float p50 = 0.0f, p95 = 0.0f, p99 = 0.0f, low1 = 0.0f;
            if (s_ft_count > 0) {
                qsort(s_ft_samples, s_ft_count, sizeof(float), compare_floats);
                p50 = s_ft_samples[(int)(s_ft_count * 0.50f)];
                p95 = s_ft_samples[(int)(s_ft_count * 0.95f)];
                p99 = s_ft_samples[(int)(s_ft_count * 0.99f)];
                low1 = (p99 > 0.001f) ? (1000.0f / p99) : 0.0f;
            }
            LOG("[vk] t=%.1f frame %llu %.1f fps [p50=%.1fms p95=%.1fms p99=%.1fms 1%%low=%.1ffps] max %.0fms level %s (%s)\n",
                now, (unsigned long long)g_dev_state.total_frames,
                g_dev_state.frames / (now - g_dev_state.last_fps_time),
                p50, p95, p99, low1,
                max_gap * 1000.0, "-", renderer_stats());
        }
        s_ft_count = 0;
        max_gap = 0;
        g_dev_state.frames = 0;
        g_dev_state.last_fps_time = now;
    }
}
// [extra]
