// [extra]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_time.h>
#include "../../../../settings.h"
#include "../../exec_internal.h"
#include "../../../renderer.h"

// ================================================================== GPU warm-up (AC_GPU_WARM=<shader folder>)
// The launcher runs the game's executable in this mode before the game, with the game closed (the NVIDIA driver files
// its Vulkan cache under the executable's name, so it is the game's own cache that fills). It compiles, for this GPU
// and driver, what the game would otherwise compile while it plays: the programs of the recorded variants
// (cache/programs.bin) and their shader libraries, a shader library for every vertex and fragment shader of the game
// (game/shaders, the keys nearly all draws use), and every recorded pipeline (cache/pipelines.bin). The results go to
// the driver's cache and cache/vk_pipelines.bin; in the game the same builds are then cache hits (well under a
// millisecond). AC_GPU_WARM_STATE: the launcher's description of the inputs; with this GPU and driver it is kept in
// cache/gpu_warm.txt, and an unchanged state skips the work. Prints "progress <done> <total>" and "done ..." lines.
// AC_GPU_WARM_PREV and AC_GPU_WARM_FROM (shader_cache.py, when the build is the same and the record files only grew,
// as the game appends to them in play): the state the last warm-up would have kept with the record files as they were
// then, and their sizes then ("<programs.bin bytes> <pipelines.bin bytes>"). If that is the state on file, only the
// records added since are compiled, with the programs their new pipelines need (the rest is in the caches already).
typedef struct { uint8_t *code; size_t size; uint32_t const_offset; int stage; uint64_t hash; } GwCode;
static GwCode *gw_codes;
static int gw_ncodes, gw_cap;
static _Atomic int gw_done;

static uint8_t *gw_read(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = n > 0 ? malloc((size_t)n + 0x10000) : NULL;  // (zero tail: the translator may look past the code)
    if (d) { memset(d + n, 0, 0x10000); if (fread(d, 1, (size_t)n, f) != (size_t)n) { free(d); d = NULL; } }
    fclose(f);
    if (d) *size = (size_t)n;
    return d;
}

// <name>.code / <name>.control pairs (astral_chain/shader_cache.py), as shader_precompile.c load_codes reads them
static void gw_file(const char *path, void *ctx) {
    (void)ctx;
    size_t len = strlen(path);
    if (len < 6 || strcmp(path + len - 5, ".code")) return;
    char cpath[1024];
    snprintf(cpath, sizeof cpath, "%.*s.control", (int)(len - 5), path);
    size_t size = 0, csize = 0;
    uint8_t *code = gw_read(path, &size), *control = gw_read(cpath, &csize);
    if (!code || !control || csize < 0x704) { free(code); free(control); return; }
    uint32_t co, magic = 0, sph0 = 0;
    memcpy(&co, control + 0x700, 4);
    free(control);
    if (size >= 0x34) { memcpy(&magic, code, 4); memcpy(&sph0, code + 0x30, 4); }
    int type = (sph0 >> 10) & 0xF, stage = magic != 0x12345678 ? 5 : type == 1 ? 0 : type == 5 ? 1 : -1;
    if ((stage != 0 && stage != 1) || !co || co > size) { free(code); return; }
    if (gw_ncodes == gw_cap) { gw_cap = gw_cap ? gw_cap * 2 : 1024; gw_codes = realloc(gw_codes, (size_t)gw_cap * sizeof *gw_codes); }
    gw_codes[gw_ncodes++] = (GwCode){code, size, co, stage, hmix(0x9E3779B97F4A7C15ull + (uint64_t)stage, code, co)};
}

static void gw_load(const char *folder) {
    extern void vk_walk_files(const char *folder, void (*fn)(const char *path, void *ctx), void *ctx);
    vk_walk_files(folder, gw_file, NULL);
}

static GwCode *gw_find(uint64_t hash, int stage) {
    for (int i = 0; i < gw_ncodes; i++) if (gw_codes[i].stage == stage && gw_codes[i].hash == hash) return &gw_codes[i];
    return NULL;
}

// records of cache/programs.bin within its first bytes, counted as warm_load reads them
static int gw_old_programs(long bytes) { // [extra]
    return warm_old_programs(bytes); // [extra]
} // [extra]
// prec entries made from the records within the first bytes of cache/pipelines.bin (prec_load: in file order)
static int gw_old_pipelines(long bytes) { // [extra]
    return prec_old_pipelines(bytes); // [extra]
} // [extra]

static void gw_stage_job(void *arg) {
    gpl_warm_job(arg);  // (frees arg)
    atomic_fetch_add(&gw_done, 1);
}

typedef struct { PipeKey k; } GwPipe;
static void gw_pipe_job(void *arg) {
    GwPipe *g = arg;
    VkPipeline p = VK_NULL_HANDLE;
    if (create_pipeline(&g->k, &p) == VK_SUCCESS) vkDestroyPipeline(g_vk.dev, p, NULL);
    free(g);
    atomic_fetch_add(&gw_done, 1);
}

int vk_gpu_warm(const char *folder) {
    extern void vk_init_for_warm(void);
    extern void vk_pipeline_cache_save(void);
    uint64_t t0 = SDL_GetTicksNS();
    vk_init_for_warm();
    char gpu[512], state[1024], old[1024] = {0};
    snprintf(gpu, sizeof gpu, " | %s %u.%u.%u (%08x) | shader libraries %d", g_vk.props.deviceName,
             VK_API_VERSION_MAJOR(g_vk.props.driverVersion), VK_API_VERSION_MINOR(g_vk.props.driverVersion),
             VK_API_VERSION_PATCH(g_vk.props.driverVersion), g_vk.props.driverVersion, g_vk.gpl);
    snprintf(state, sizeof state, "%s%s", getenv("AC_GPU_WARM_STATE") ? getenv("AC_GPU_WARM_STATE") : "", gpu);
    {
        FILE *f = fopen("cache/gpu_warm.txt", "rb");
        if (f) { size_t n = fread(old, 1, sizeof old - 1, f); old[n] = 0; fclose(f); }
        if (!strcmp(old, state)) { printf("done skipped (unchanged)\n"); fflush(stdout); return 0; }
    }
    long from_prog = -1, from_pipe = -1;
    {
        const char *prev = getenv("AC_GPU_WARM_PREV"), *from = getenv("AC_GPU_WARM_FROM");
        char was[1024];
        if (prev && from) snprintf(was, sizeof was, "%s%s", prev, gpu);
        if (!prev || !from || strcmp(old, was) || sscanf(from, "%ld %ld", &from_prog, &from_pipe) != 2) from_prog = from_pipe = -1;
    }
    gw_load(folder);
    if (!gw_ncodes) { printf("done failed (no shaders in %s)\n", folder); fflush(stdout); return 1; }
    warm_load();
    prec_load();
    bool added = from_prog >= 0;  // (only what was added since the last warm-up)
    int old_progs = added ? gw_old_programs(from_prog) : 0, old_pipes = added ? gw_old_pipelines(from_pipe) : 0;
    // programs of the recorded variants (their shader libraries come with them: compile_program)
    Prog **progs = calloc((size_t)(warm_count() > 0 ? warm_count() : 1), sizeof *progs); // [extra]
    int nprogs = 0;
    for (int i = 0; i < warm_count(); i++) { // [extra]
        const WarmRec *wr = warm_entry(i); // [extra]
        if (!wr) continue; // [extra]
        if (i < old_progs) {  // (an older one: only if a new pipeline needs it)
            uint64_t sh[2] = {wr->vh, wr->fh}; // [extra]
            uint64_t ident = hmix(hmix(hmix(0xcbf29ce484222325ull, sh, sizeof sh), &wr->vkey, sizeof wr->vkey), // [extra]
                                  &wr->fkey, sizeof wr->fkey); // [extra]
            bool needed = prec_ident_needed_from(ident, old_pipes); // [extra]
            if (!needed) continue;
        }
        GwCode *vs = gw_find(wr->vh, 0), *fs = wr->fh ? gw_find(wr->fh, 1) : NULL; // [extra]
        if (!vs || (wr->fh && !fs)) continue; // [extra]
        NvnProgram pair;
        memset(&pair, 0, sizeof pair);
        pair.stages[0].code_addr = (uint64_t)(uintptr_t)vs->code;
        pair.stages[0].const_offset = vs->const_offset;
        pair.stage_mask = 1;
        if (fs) { pair.stages[1].code_addr = (uint64_t)(uintptr_t)fs->code; pair.stages[1].const_offset = fs->const_offset; pair.stage_mask |= 2; }
        ShaderKey vk = wr->vkey, fk = wr->fkey; // [extra]
        Prog *p = get_program_ex(&pair, &vk, &fk, false);
        if (p) progs[nprogs++] = p;
    }
    // a shader library for every shader, as the game builds them when it loads one (gpl_warm_stage)
    int nstage = 0;
    if (g_vk.gpl && !added) {
        for (int i = 0; i < gw_ncodes; i++) {
            GplWarm *w = malloc(sizeof *w);
            w->code = (uint64_t)(uintptr_t)gw_codes[i].code;
            w->cmax = gw_codes[i].const_offset;
            w->stage = gw_codes[i].stage;
            run_job(gw_stage_job, w);
            nstage++;
        }
    }
    int total = nprogs + nstage + (prec_count() > old_pipes ? prec_count() - old_pipes : 0), queued_pipes = 0, last = -1; // [extra]
    printf("warm-up: %d shaders, %d recorded programs, %d recorded pipelines%s\n", gw_ncodes, nprogs, total - nprogs - nstage,
           added ? " (added since the last time)" : "");
    fflush(stdout);
    // the recorded pipelines, once their program is ready
    bool *queued = calloc((size_t)(prec_count() > 0 ? prec_count() : 1), 1); // [extra]
    for (int r = 0; r < old_pipes && r < prec_count(); r++) queued[r] = true; // [extra]
    for (;;) {
        int ready = 0;
        for (int i = 0; i < nprogs; i++) {
            if (atomic_load(&progs[i]->state) == 0) continue;
            ready++;
            if (atomic_load(&progs[i]->state) != 1) continue;
            for (int r = 0; r < prec_count(); r++) { // [extra]
                const PipeRec *pe = prec_entry(r); // [extra]
                if (!pe || queued[r] || pe->ident != progs[i]->ident) continue; // [extra]
                queued[r] = true;
                GwPipe *g = malloc(sizeof *g);
                g->k = pe->k; // [extra]
                if (!pipe_key_for_device(&g->k)) { free(g); atomic_fetch_add(&gw_done, 1); continue; }
                g->k.prog = progs[i];
                run_job(gw_pipe_job, g);
                queued_pipes++;
            }
        }
        int done = ready + atomic_load(&gw_done);
        if (done / 25 != last / 25) { last = done; printf("progress %d %d\n", done, total); fflush(stdout); }
        int pipes_done = 0;
        for (int r = old_pipes; r < prec_count(); r++) pipes_done += queued[r]; // [extra]
        if (ready == nprogs && atomic_load(&gw_done) >= nstage + pipes_done && SDL_GetTicksNS() - t0 > 1000000ull) {
            // (records whose program never got made: counted as done)
            if (done < total) { printf("progress %d %d\n", total, total); fflush(stdout); }
            break;
        }
        SDL_Delay(20);
    }
    vk_pipeline_cache_save();
    FILE *f = fopen("cache/gpu_warm.txt", "wb");
    if (f) { fputs(state, f); fclose(f); }
    printf("done %d programs, %d shader libraries, %d pipelines in %.1f s%s\n", nprogs, nstage, queued_pipes, (SDL_GetTicksNS() - t0) / 1e9,
           added ? " (added since the last time)" : "");
    fflush(stdout);
    return 0;
}
