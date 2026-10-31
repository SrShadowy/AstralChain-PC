// [extra]
#include <SDL3/SDL_timer.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../../kernel.h"
#include "../../exec_internal.h"

// Program build statistics
_Atomic unsigned long long build_ns_translate, build_ns_spirv;
uint64_t warm_stat_queued, warm_stat_ready, warm_stat_late;

VkPipelineLayout make_layout(Prog *p, int nsets, VkShaderStageFlags s0, VkShaderStageFlags s1) {
    p->set_layout[0] = vk_set_layout(p->binds, p->nbinds, 0, s0);
    if (nsets > 1) p->set_layout[1] = vk_set_layout(p->binds, p->nbinds, 1, s1);
    VkPipelineLayoutCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = (uint32_t)nsets,
                                     .pSetLayouts = p->set_layout};
    if (g_vk.gpl && nsets == 2) ci.flags = VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT;
    VkPipelineLayout l;
    VK_CHECK(vkCreatePipelineLayout(g_vk.dev, &ci, NULL, &l));
    return l;
}

void save_glsl(const char *src, uint64_t addr, const char *suffix) {
    if (!getenv("LARECOMP_SAVE_MSL")) return;
    mkdir("build", 0755);
    mkdir("build/glsl", 0755);
    char path[256];
    snprintf(path, sizeof path, "build/glsl/%llx_%s.glsl", (unsigned long long)addr, suffix);
    FILE *f = fopen(path, "w");
    if (f) { fputs(src, f); fclose(f); }
}

void save_shader_input(uint64_t addr, uint64_t size, const ShaderKey *key, const char *suffix) {
    if (!getenv("LARECOMP_SAVE_MSL") || !addr) return;
    mkdir("build", 0755);
    mkdir("build/glsl", 0755);
    char path[256];
    snprintf(path, sizeof path, "build/glsl/%llx_%s.bin", (unsigned long long)addr, suffix);
    FILE *f = fopen(path, "wb");
    if (f) { fwrite((const void *)(uintptr_t)addr, 1, size > 0x10000 ? 0x10000 : size, f); fclose(f); }
    snprintf(path, sizeof path, "build/glsl/%llx_%s.key", (unsigned long long)addr, suffix);
    f = fopen(path, "wb");
    if (f) { fwrite(key, 1, sizeof *key, f); fclose(f); }
}

static void compile_program(void *arg) {
    Prog *p = arg;
    size_t nv = 0, nf = 0;
    uint64_t bt0 = SDL_GetTicksNS();
    uint32_t *sv = vk_glsl_to_spirv(p->vi->msl, STAGE_VERTEX, "vs", &nv);
    uint32_t *sf = sv ? vk_glsl_to_spirv(p->fi->msl, STAGE_FRAGMENT, "fs", &nf) : NULL;
    atomic_fetch_add(&build_ns_spirv, SDL_GetTicksNS() - bt0);
    if (!sv || !sf) { free(sv); free(sf); atomic_store(&p->state, 2); return; }
    p->nbinds = vk_reflect(sv, nv, p->binds, MAX_BIND);
    p->nbinds += vk_reflect(sf, nf, p->binds + p->nbinds, MAX_BIND - p->nbinds);
    p->vs = vk_module(sv, nv);
    p->fs = vk_module(sf, nf);
    StageLib *lv = gpl_stage(0, sv, nv), *lf = lv ? gpl_stage(1, sf, nf) : NULL;
    if (lv && lf) { p->vs_lib = lv->lib; p->fs_lib = lf->lib; }
    free(sv);
    free(sf);
    p->layout = make_layout(p, 2, VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT);
    atomic_fetch_add(&stat_compiles, 1); atomic_fetch_add(&vk_pipelines_built, 1);
    atomic_store(&p->state, p->vs && p->fs ? 1 : 2);
}

static void translate_program(void *arg) {
    Prog *p = arg;
    maxwell_gmem_base = HEAP_BASE;
    maxwell_gmem_size = HEAP_MAX;
    uint64_t bt0 = SDL_GetTicksNS();
    pthread_mutex_lock(&translate_lock);
    save_shader_input(p->code[0], p->cmax[0], &p->vkey, "vs");
    p->vi = maxwell_translate((const uint8_t *)(uintptr_t)p->code[0], p->cmax[0], STAGE_VERTEX, &p->vkey);
    if (p->vi->msl) {
        p->fkey.vs_out_mask = p->vi->out_mask;
        if (p->code[1]) {
            save_shader_input(p->code[1], p->cmax[1], &p->fkey, "fs");
            p->fi = maxwell_translate((const uint8_t *)(uintptr_t)p->code[1], p->cmax[1], STAGE_FRAGMENT, &p->fkey);
        } else {
            p->fi = calloc(1, sizeof *p->fi);
            p->fi->stage = STAGE_FRAGMENT;
            p->fi->msl = strdup("#version 460\nvoid main() {}\n");
        }
    }
    pthread_mutex_unlock(&translate_lock);
    atomic_fetch_add(&build_ns_translate, SDL_GetTicksNS() - bt0);
    if (!p->vi->msl) {
        LOG("[shader] VS translate failed: %s (code %llx)\n", p->vi->error, (unsigned long long)p->code[0]);
        atomic_store(&p->state, 2);
        return;
    }
    if (!p->fi->msl) {
        LOG("[shader] FS translate failed: %s\n", p->fi->error);
        atomic_store(&p->state, 2);
        return;
    }
    p->vs_uses_vid = strstr(p->vi->msl, "gl_VertexIndex") != NULL;
    save_glsl(p->vi->msl, p->code[0], "vs");
    save_glsl(p->fi->msl, p->code[1], "fs");
    run_job(compile_program, p);
}

#define PROG_TAB 4096
static struct { uint64_t key; Prog *prog; } prog_tab[PROG_TAB];

#define WARM_MAX 16384
#define WARM_MAGIC (0x4D524157u ^ (uint32_t)sizeof(WarmRec))
static WarmRec warm_rec[WARM_MAX];
static int warm_n = -1;
static FILE *warm_out;

static bool warm_off(void) {
    static int off = -1;
    if (off < 0) off = getenv("LARECOMP_NO_WARM") != NULL || sync_shaders();
    return off;
}

uint64_t stage_hash(const NvnProgram *np, int i) {
    if (!(np->stage_mask & (1u << i)) || !np->stages[i].code_addr) return 0;
    uint64_t n = np->stages[i].const_offset ? np->stages[i].const_offset : 0x10000;
    return hmix(0x9E3779B97F4A7C15ull + (uint64_t)i, (const void *)(uintptr_t)np->stages[i].code_addr, (size_t)n);
}

void warm_load(void) {
    warm_n = 0;
    FILE *f = fopen("cache/programs.bin", "rb");
    if (!f) return;
    uint32_t magic = 0;
    if (fread(&magic, 4, 1, f) == 1 && magic == WARM_MAGIC)
        for (uint32_t word; warm_n < WARM_MAX && fread(&word, 4, 1, f) == 1;) {
            if (word == WARM_MAGIC) continue;
            memcpy(&warm_rec[warm_n], &word, 4);
            if (fread((uint8_t *)&warm_rec[warm_n] + 4, sizeof warm_rec[0] - 4, 1, f) != 1) break;
            warm_n++;
        }
    fclose(f);
    if (magic != WARM_MAGIC) remove("cache/programs.bin");
    LOG("[vk] program warm-up: %d recorded variants\n", warm_n);
}

int warm_count(void) {
    return warm_n;
}

const WarmRec *warm_entry(int i) {
    if (i >= 0 && i < warm_n) return &warm_rec[i];
    return NULL;
}

int warm_old_programs(long bytes) {
    FILE *f = fopen("cache/programs.bin", "rb");
    if (!f) return 0;
    int n = 0;
    uint32_t magic = 0;
    if (fread(&magic, 4, 1, f) == 1 && magic == WARM_MAGIC)
        for (uint32_t word; n < warm_n && fread(&word, 4, 1, f) == 1;) {
            if (word == WARM_MAGIC) continue;
            if (fseek(f, (long)sizeof(WarmRec) - 4, SEEK_CUR) || ftell(f) > bytes) break;
            n++;
        }
    fclose(f);
    return n;
}

static void warm_record(const NvnProgram *np, const ShaderKey *vkey, const ShaderKey *fkey) {
    if (warm_off()) return;
    if (warm_n < 0) warm_load();
    WarmRec r;
    memset(&r, 0, sizeof r);
    r.vh = stage_hash(np, 0);
    r.fh = stage_hash(np, 1);
    r.vkey = *vkey;
    r.fkey = *fkey;
    for (int i = 0; i < warm_n; i++) if (!memcmp(&warm_rec[i], &r, sizeof r)) return;
    if (warm_n == WARM_MAX) return;
    warm_rec[warm_n++] = r;
    if (!warm_out) {
        mkdir("cache", 0755);
        warm_out = fopen("cache/programs.bin", "ab");
        if (warm_out && fseek(warm_out, 0, SEEK_END) == 0 && ftell(warm_out) == 0) { uint32_t magic = WARM_MAGIC; fwrite(&magic, 4, 1, warm_out); }
    }
    if (warm_out) { fwrite(&r, sizeof r, 1, warm_out); fflush(warm_out); }
}

#define WARM_SEEN 8192
static struct { uint64_t hash; NvnShaderStage stage; } warm_seen[WARM_SEEN];
static int warm_nseen;
static const NvnShaderStage *warm_seen_stage(uint64_t h) {
    for (int i = warm_nseen - 1; i >= 0; i--) if (warm_seen[i].hash == h) return &warm_seen[i].stage;
    return NULL;
}

static void warm_program(NvnProgram *np) {
    for (int s = 0; s < 2; s++) if (np->stage_mask & (1u << s)) gpl_warm_stage(np, s, stage_hash(np, s));
    if (warm_off()) return;
    if (np->stage_mask & (1u << 5)) { warm_compute(np); return; }
    if (warm_n < 0) warm_load();
    if (!warm_n) return;
    for (int s = 0; s < 2; s++) {
        if (!(np->stage_mask & (1u << s))) continue;
        uint64_t h = stage_hash(np, s);
        if (warm_nseen < WARM_SEEN && !warm_seen_stage(h)) {
            warm_seen[warm_nseen].hash = h;
            warm_seen[warm_nseen++].stage = np->stages[s];
        }
        for (int i = 0; i < warm_n; i++) {
            if ((s == 0 ? warm_rec[i].vh : warm_rec[i].fh) != h) continue;
            const NvnShaderStage *vs = warm_seen_stage(warm_rec[i].vh);
            const NvnShaderStage *fs = warm_rec[i].fh ? warm_seen_stage(warm_rec[i].fh) : NULL;
            if (!vs || (warm_rec[i].fh && !fs)) continue;
            NvnProgram pair;
            memset(&pair, 0, sizeof pair);
            pair.stages[0] = *vs;
            pair.stage_mask = 1;
            if (fs) { pair.stages[1] = *fs; pair.stage_mask |= 2; }
            ShaderKey vk = warm_rec[i].vkey, fk = warm_rec[i].fkey;
            get_program_ex(&pair, &vk, &fk, false);
        }
    }
}

Prog *get_program_ex(NvnProgram *np, ShaderKey *vkey, ShaderKey *fkey, bool from_draw) {
    if (!(np->stage_mask & 1)) {
        static int nlog;
        if (nlog++ < 10) LOG("[shader] program with stage mask %#x skipped\n", np->stage_mask);
        return NULL;
    }
    uint64_t h = 0xcbf29ce484222325ull;
    for (int i = 0; i < 2; i++) { uint64_t a = (np->stage_mask & (1u << i)) ? np->stages[i].code_addr : 0; for (int k = 0; k < 8; k++) { h ^= (a >> (k * 8)) & 0xFF; h *= 0x100000001B3ull; } }
    const uint8_t *vb = (const uint8_t *)vkey;
    for (size_t i = 0; i < sizeof *vkey; i++) { h ^= vb[i]; h *= 0x100000001B3ull; }
    for (int i = 0; i < 8; i++) { h ^= fkey->rt_kind[i]; h *= 0x100000001B3ull; }
    const uint8_t at[3] = {fkey->alpha_func, (uint8_t)fkey->alpha_ref, (uint8_t)(fkey->alpha_ref >> 8)};
    for (int i = 0; i < 3; i++) { h ^= at[i]; h *= 0x100000001B3ull; }
    if (!h) h = 1;
    uint32_t slot = (uint32_t)(h ^ (h >> 31)) & (PROG_TAB - 1);
    while (prog_tab[slot].key && prog_tab[slot].key != h) slot = (slot + 1) & (PROG_TAB - 1);
    if (prog_tab[slot].key) return prog_tab[slot].prog;
    Prog *p = calloc(1, sizeof *p);
    prog_tab[slot].key = h;
    prog_tab[slot].prog = p;
    for (int i = 0; i < 2; i++) {
        p->code[i] = (np->stage_mask & (1u << i)) ? np->stages[i].code_addr : 0;
        p->cmax[i] = np->stages[i].const_offset ? np->stages[i].const_offset : 0x10000;
    }
    p->vkey = *vkey;
    p->fkey = *fkey;
    uint64_t sh[2] = {stage_hash(np, 0), stage_hash(np, 1)};
    p->ident = hmix(hmix(hmix(0xcbf29ce484222325ull, sh, sizeof sh), vkey, sizeof *vkey), fkey, sizeof *fkey);
    pipe_warm_add(p);
    p->warmed = !from_draw;
    if (from_draw) warm_record(np, vkey, fkey);
    else warm_stat_queued++;
    run_translate(translate_program, p);
    return p;
}

Prog *get_program(NvnProgram *np, ShaderKey *vkey, ShaderKey *fkey) {
    Prog *p = get_program_ex(np, vkey, fkey, true);
    if (p && !p->drawn) {
        p->drawn = true;
        if (p->warmed) { if (atomic_load(&p->state) == 0) warm_stat_late++; else warm_stat_ready++; }
    }
    if (p && atomic_load(&p->state) == 0) {
        promote_translate_job(p);
        promote_worker_job(p);
    }
    return p;
}

void prog_reset(void) {
    memset(prog_tab, 0, sizeof prog_tab);
}

void vk_program_init(void) {
    extern void (*nvn_program_warm_hook)(NvnProgram *np);
    nvn_program_warm_hook = warm_program;
}
// [extra]
