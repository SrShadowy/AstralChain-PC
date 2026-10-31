// [extra]
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <SDL3/SDL_timer.h>
#include "vk.h"
#include "shaders.h"

// Core hashing helpers
static inline uint64_t hash_words(uint64_t h, const void *p, size_t n) {
    const uint8_t *b = (const uint8_t *)p;
    for (; n >= 8; n -= 8, b += 8) { uint64_t w; memcpy(&w, b, 8); h = (h ^ w) * 0x9E3779B97F4A7C15ull; h ^= h >> 29; }
    for (; n; n--, b++) { h = (h ^ *b) * 0x100000001B3ull; }
    return h ^ (h >> 32);
}

static inline uint64_t hmix(uint64_t h, const void *p, size_t n) {
    const uint8_t *b = (const uint8_t *)p;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 0x100000001B3ull; }
    return h;
}

// Shared upload telemetry counters
extern uint64_t stat_upload_bytes;
extern uint64_t stat_up_vtx;
extern uint64_t stat_up_cbuf;
extern uint64_t stat_passes;

// Submissions sequence tracking
extern uint64_t submit_seq;

// Transient dedupe slots and function
enum { DEDUPE_RSCALE_V, DEDUPE_RSCALE_F, DEDUPE_TINT, DEDUPE_DRV_V, DEDUPE_DRV_F, DEDUPE_DRV_C, DEDUPE_SLOTS };
VkSpan copy_dedupe(int slot, const void *data, uint64_t size, uint64_t align);
void ring_retire(void);

// Pipeline and translation workers
#include <pthread.h>
extern pthread_mutex_t translate_lock;
bool sync_shaders(void);
void workers_start(void);
void run_job(void (*fn)(void *), void *arg);
void run_job_low(void (*fn)(void *), void *arg);
void run_translate(void (*fn)(void *), void *arg);
void promote_worker_job(void *arg);
void promote_translate_job(void *arg);
bool skip_unbuilt(void);
void wait_built(_Atomic int *state);
extern uint64_t cum_built_ns;

// Programs and pipelines
#define MAX_BIND 96
typedef struct Prog {
    ShaderInfo *vi, *fi;
    uint64_t code[2], cmax[2];
    ShaderKey vkey, fkey;
    VkShaderModule vs, fs;
    VkBindingInfo binds[MAX_BIND];
    int nbinds;
    VkDescriptorSetLayout set_layout[2];
    VkPipelineLayout layout;
    bool vs_uses_vid;
    bool warmed, drawn;
    uint64_t ident;
    VkPipeline vs_lib, fs_lib;
    _Atomic int state;
} Prog;

#define ZERO_BINDING 16
typedef struct PipeKey {
    Prog *prog;
    VkFormat color[8], depth, stencil;
    uint32_t ncolor, samples;
    uint32_t attr_fmt[32], attr_off[32], attr_bind[32];
    uint32_t attr_mask;
    uint32_t stride[17], divisor[17], used_streams;
    uint8_t blend[8][8];
    uint8_t blend_on, a2c, topo_class, poly_fill;
    uint32_t write_mask[8];
} PipeKey;

typedef struct Pipe {
    PipeKey k;
    uint64_t hash;
    VkPipeline pipe;
    _Atomic int state, claimed;
    bool warmed, drawn;
    _Atomic uint64_t opt;
} Pipe;

typedef struct { uint64_t ident; PipeKey k; } PipeRec;

// Pipeline contracts
extern _Atomic int stat_compiles;
extern _Atomic unsigned long long build_ns_pipe;
extern _Atomic unsigned long long build_pipe_hits, build_pipe_misses, build_ns_pipe_miss;
extern unsigned long long pwarm_queued, pwarm_ready, pwarm_late, pwarm_rejected;
extern _Atomic unsigned long long gpl_linked, gpl_link_ns, gpl_opt_built;

bool pipe_key_for_device(PipeKey *k);
VkResult create_pipeline(const PipeKey *k, VkPipeline *out);
void build_pipeline(void *arg);
bool gpl_link(Pipe *pp);
void pipe_record(const Prog *p, const PipeKey *k);
void pipe_warm_add(Prog *p);
void vk_pipeline_warmup(void);
Pipe *pipe_lookup(const PipeKey *k, Prog *pr);
void pipe_reset(void);

// Pipeline warm-up query contracts for offline/GPU warmup
void prec_load(void);
int prec_count(void);
const PipeRec *prec_entry(int i);
int prec_old_pipelines(long bytes);
bool prec_ident_needed_from(uint64_t ident, int old_pipes);

// Program contracts
typedef struct { uint64_t vh, fh; ShaderKey vkey, fkey; } WarmRec;

extern _Atomic unsigned long long build_ns_translate, build_ns_spirv;
extern uint64_t warm_stat_queued, warm_stat_ready, warm_stat_late;

VkPipelineLayout make_layout(Prog *p, int nsets, VkShaderStageFlags s0, VkShaderStageFlags s1);
Prog *get_program(NvnProgram *np, ShaderKey *vkey, ShaderKey *fkey);
Prog *get_program_ex(NvnProgram *np, ShaderKey *vkey, ShaderKey *fkey, bool from_draw);
uint64_t stage_hash(const NvnProgram *np, int i);
void prog_reset(void);
void vk_program_init(void);
void save_glsl(const char *src, uint64_t addr, const char *suffix);
void save_shader_input(uint64_t addr, uint64_t size, const ShaderKey *key, const char *suffix);

// Program warm-up query contracts for offline/GPU warmup
void warm_load(void);
int warm_count(void);
const WarmRec *warm_entry(int i);
int warm_old_programs(long bytes);

// Compute program warm-up forward contract
void warm_compute(NvnProgram *np);

// Format and fixed-function mapping tables
static const VkDynamicState DYN[] = {
    VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_BIAS, VK_DYNAMIC_STATE_BLEND_CONSTANTS,
    VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK, VK_DYNAMIC_STATE_STENCIL_WRITE_MASK, VK_DYNAMIC_STATE_STENCIL_REFERENCE,
    VK_DYNAMIC_STATE_CULL_MODE, VK_DYNAMIC_STATE_FRONT_FACE, VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
    VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE, VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE, VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
    VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE, VK_DYNAMIC_STATE_STENCIL_OP, VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE,
    VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE};
#define NDYN (sizeof DYN / sizeof DYN[0])

int attr_kind(uint32_t fmt);
VkFormat vertex_format(uint32_t f);
VkFormat vertex_format_supported(VkFormat f);
VkBlendFactor blend_factor(uint8_t f);
VkBlendOp blend_op(uint8_t e);
VkCompareOp compare_fn(uint8_t f);
VkStencilOp stencil_op(uint8_t o);
int rt_kind(uint32_t f);

// Cache contracts
VkSpan guest_copy_vtx(uint64_t addr, uint64_t size);
void guest_vcopy_flush(void);
VkSpan guest_copy(uint64_t addr, uint64_t size);
void guest_copy_flush(void);

// Execution state and pipeline state machine
typedef struct {
    NvnTexture *color[8];
    NvnTextureView cview[8];
    uint8_t has_cview[8];
    int ncolor;
    NvnTexture *depth;
    NvnTextureView dview;
    uint8_t has_dview;
} RtState;

typedef struct { uint64_t addr, size; } Range;

typedef struct ExecState {
    RtState rt;
    bool rt_dirty;
    NvnProgram *program;
    uint8_t blend[8][8];
    uint32_t channel_mask;
    uint8_t color_state[4], ds[8], ms[0x18], poly[4];
    float poly_offset[3];
    float alpha_ref;
    uint32_t attrib[16];
    int nattrib;
    uint32_t stream[16][2];
    int nstream;
    Range vbuf[16], ubuf[6][18], sbuf[6][16];
    uint64_t tex[6][32], img[6][8];
    NvnDescPool *tpool, *spool;
    float viewport[16][4];
    int32_t scissor[16][4];
    float depth_range[16][2];
    float blend_color[4];
    uint32_t stencil_ref[2], stencil_mask[2], stencil_vmask[2];
    bool prim_restart;
    uint32_t restart_index;
    uint64_t copy_row_stride, copy_image_stride;
} ExecState;

extern ExecState st;
extern uint32_t vk_view_gen;
extern int naliases;

bool ct_on(void);
void gp_mark(VkCommandBuffer cb, const char *fmt, ...);

// Copy elision
int copy_opt(void);
bool alias_make(NvnTexture *src, NvnTexture *dst, TexBackend *s, TexBackend *d);
void alias_before_write(NvnTexture *t, VkImageAspectFlags full, const char *why);
NvnTexture *alias_read(NvnTexture *t);
void alias_draw_check(void);
void vk_alias_texture_destroyed(NvnTexture *t);
void vk_alias_reset(void);
void vk_alias_stats(void);
// Shader library contracts (GPL)
typedef struct { uint64_t key; VkPipeline lib; _Atomic int state; } StageLib;
extern _Atomic unsigned long long gpl_stage_built, gpl_stage_hits, gpl_stage_ns;
extern _Atomic unsigned long long vk_pipelines_built;
typedef struct { uint64_t code, cmax; int stage; } GplWarm;
VkBool32 game_depth_clamp(void);
StageLib *gpl_stage(int stage, const uint32_t *spv, size_t n);
void gpl_warm_job(void *arg);
void gpl_warm_stage(const NvnProgram *np, int s, uint64_t hash);

// Redundant-state filter
enum { SS_PIPE = 1 << 0, SS_DT = 1 << 1, SS_DW = 1 << 2, SS_DOP = 1 << 3, SS_STEN = 1 << 4, SS_SFACE0 = 1 << 5, SS_SFACE1 = 1 << 6,
       SS_BC = 1 << 7, SS_CULL = 1 << 8, SS_FF = 1 << 9, SS_TOPO = 1 << 10, SS_PRE = 1 << 11, SS_DBE = 1 << 12, SS_DB = 1 << 13,
       SS_VP = 1 << 14, SS_SC = 1 << 15, SS_IB = 1 << 16 };
typedef struct StateFilter {
    uint32_t known;  // SS_* bits: the value below is what the command buffer has
    uint32_t vb_known;  // per vertex binding
    VkPipeline pipe;
    VkBool32 dt, dw, sten, pre, dbe;
    VkCompareOp dop;
    uint32_t sface[2][7];  // fail, pass, depth fail, compare, compare mask, write mask, reference
    float bc[4], db[3];
    VkCullModeFlags cull;
    VkFrontFace ff;
    VkPrimitiveTopology topo;
    VkViewport vp;
    VkRect2D sc;
    VkBuffer ib; VkDeviceSize io; VkIndexType it;
    VkBuffer vb[17]; VkDeviceSize vo[17];
} StateFilter;

extern StateFilter ss;
extern int ss_on;
bool ss_enabled(void);
static inline bool ss_need(uint32_t bit, bool same) {
    if (ss_on < 0) ss_enabled();
    if (ss_on && (ss.known & bit) && same) return false;
    ss.known |= bit;
    return true;
}

static inline uint32_t level_extent(uint32_t v, uint32_t lv) { v >>= lv; return v ? v : 1; }

// Render pass state and commands
extern VkFormat pass_color[8], pass_depth, pass_stencil;
extern uint32_t pass_ncolor, pass_samples, pass_w, pass_h;
extern float pass_sx, pass_sy;
extern float vp_sx, vp_sy, vp_ox, vp_oy;
extern bool in_pass;
extern int debug_pass_no;
extern int prof;
extern uint32_t prof_end_line[4096];
extern int dl_on;
void dl_note(const char *fmt, ...);
extern bool g_debug_frame;
extern bool g_gpuprof_frame;

void renderer_texture_scale(NvnTexture *t, float *sx, float *sy);
bool renderer_texture_presented(NvnTexture *t);
bool renderer_texture_first_write(NvnTexture *t);
bool renderer_texture_level_fresh(NvnTexture *t);
bool renderer_texture_frame_fresh(NvnTexture *t);
void renderer_texture_mark_presented(NvnTexture *t);
uint64_t renderer_present_count(void);
void renderer_save_texture_tex(TexBackend *b, const char *path);
bool renderer_format_is_depth(uint32_t f);
void renderer_mark_gpu_owned(NvnTexture *t);
void renderer_flush(void);

void vk_end_pass_at(int line);
void begin_pass(void);
void pass_size(int *w, int *h);
VkRect2D scissor_rect(void);
void apply_scissor(void);
void set_viewport(float x, float y, float w, float h, float zn, float zf);
void ss_forget(void);

// Clear commands
void clear(int index, int kind, const void *color, uint32_t mask, bool depth, float dval, bool stencil, uint32_t sval, uint32_t smask);

// Guest heap mirror contracts
extern VkBuffer gmem_buf;
extern uint64_t gmem_size;
extern uint64_t stat_mirror_bytes, stat_wb_bytes, stat_wb_ranges, stat_wb_faults;
extern int lazy_n;

void gmem_sync(void);
void gmem_wb_record(VkCommandBuffer cb, uint64_t seq);
void gmem_wb_note(uint64_t addr, uint64_t size);
void gmem_writeback_apply(uint64_t done);
VkDescriptorBufferInfo gmem_info(void);
void gmem_reset(void);
bool gmem_is_mirror(void);
void gmem_interest(uint64_t addr, uint64_t size);
bool gmem_ensure_size(uint64_t addr, uint64_t size);

static inline VkDescriptorBufferInfo ubo_span(VkSpan s, uint64_t size) {
    uint64_t max = g_vk.props.limits.maxUniformBufferRange;
    return (VkDescriptorBufferInfo){s.buf, s.off, size > max ? max : size};
}

// Gfast fast copy contracts
VkDescriptorBufferInfo gfast_info(void);
void gfast_prepare(VkCommandBuffer cb);
void gfast_finish(VkCommandBuffer cb);
VkDescriptorBufferInfo gmap_info(void);
void gfast_reset(void);

// Vertex cache & page write tracking contracts
extern uint64_t prof_vc_hits, prof_vc_bypass, prof_vc_copied, prof_vc_queries, prof_vc_scan_pages, prof_vc_scan_ns;
extern uint64_t prof_ww_direct, prof_ww_span;

int vk_pages_changed(uint64_t addr, uint64_t size, uint32_t since, uint32_t *now);
void vc_flush(void);
void vc_mark_sbufs(int stage);
bool guest_direct(uint64_t addr, uint64_t size, VkBuffer *buf, VkDeviceSize *off);
void vc_reset(void);
void ww_enter_list(void);
void ww_leave_list(void);

// Descriptors and feedback snapshots contracts // [extra]
extern uint64_t stat_feedback_copies; // [extra]
extern float tex_sc[2][64][2]; // [extra]
extern uint32_t tex_wh[2][64][2]; // [extra]
extern float g_mirror_w; // [extra]
extern const char *snap_reason; // [extra]
void snapshots_invalidate(NvnTexture *t); // [extra]
void snapshots_invalidate_reason(const char *reason, NvnTexture *t); // [extra]
void feedback_clear(void); // [extra]
NvnTexture *shader_texture_object(int stage, ShaderTexture *st_); // [extra]
void resolve_feedback(Prog *pr); // [extra]
void texture_info(int stage, ShaderInfo *si, int i, VkDescriptorImageInfo *out, float sc[2], uint32_t wh[2]); // [extra]
VkDescriptorSet stage_set(Prog *p, VkDescriptorSetLayout layout, uint32_t set, int stage, ShaderInfo *si, const float *rscale_row0,
                         const float tint[8], uint32_t gx, uint32_t gy, uint32_t gz, const VkDescriptorImageInfo *pre_ii); // [extra]

// Compute dispatch contracts // [extra]
extern uint64_t stat_dispatches, stat_skipped; // [extra]
extern uint32_t skip_lines[8192]; // [extra]
#ifndef SKIPPED // [extra]
#define SKIPPED() (stat_skipped++, skip_lines[__LINE__ & 8191]++) // [extra]
#endif // [extra]
void dispatch_compute(uint32_t gx, uint32_t gy, uint32_t gz); // [extra]
void cs_reset(void); // [extra]

// Texture & buffer copy contracts // [extra]
typedef struct { uint64_t tex; NvnTextureView view; uint8_t has_view; int32_t region[6]; } TexRefRec; // [extra]
extern __thread const char *wait_why; // [extra]
void copy_buffer_to_texture(uint64_t src, const TexRefRec *dst); // [extra]
void copy_texture_to_buffer(const TexRefRec *src, uint64_t dst); // [extra]
void copy_texture(const TexRefRec *src, const TexRefRec *dst, uint64_t flags); // [extra]

// Present contracts // [extra]
void vk_present_draw(VkCommandBuffer cb, TexBackend *b, float vx, float vy, float vw, float vh, const float pv[8]); // [extra]
void renderer_save_texture_tex(TexBackend *b, const char *path); // [extra]

// Draw contracts // [extra]
void draw(CmdRec *r, bool indexed); // [extra]
void oq_draw(VkCommandBuffer cb, bool end); // [extra]
void gp_record_draw(uint64_t fs_hash); // [extra]
uint32_t nvn_window_origin(void); // [extra]
uint32_t nvn_depth_mode(void); // [extra]
extern uint64_t stat_draws; // [extra]
extern uint64_t stat_wait_ns, stat_flushes; // [extra]
extern uint64_t stat_dispatches, stat_skipped; // [extra]
extern uint64_t stat_passes; // [extra]
extern uint64_t stat_feedback_copies, stat_upload_bytes, stat_up_vtx, stat_up_cbuf; // [extra]
extern uint64_t cum_draws, cum_exec, cum_wait, cum_submits, cum_up_ns, cum_built_ns; // [extra]
extern uint64_t vk_draws_total; // [extra]
extern uint64_t prof_exec, prof_draw, prof_sets, prof_vtx, prof_submit, prof_pre, prof_key, prof_dyn, prof_tex, prof_up, prof_gpu; // [extra]
extern uint64_t prof_views_created; // [extra]
extern uint64_t prof_up_ns; // [extra]
#ifndef PROF_T0 // [extra]
#define PROF_T0() uint64_t prof_t0_ = prof ? SDL_GetTicksNS() : 0 // [extra]
#define PROF_ADD(v) do { if (prof) v += SDL_GetTicksNS() - prof_t0_; } while (0) // [extra]
#endif // [extra]

// GPU warm-up contract // [extra]
int vk_gpu_warm(const char *folder); // [extra]

// Submission & telemetry contracts // [extra]
extern uint64_t op_counts[CMD_COUNT]; // [extra]
void vk_prof_frame(uint64_t frame, uint64_t present_ns); // [extra]
void submission_init(void); // [extra]
void handle_reset_counter(uint32_t type); // [extra]
void handle_report_counter(uint32_t type, uint64_t *out); // [extra]
void renderer_submit(void); // [extra]
uint64_t renderer_fence_seq(void); // [extra]
void vk_submit_extra(VkSemaphore wait, VkSemaphore signal, VkCommandBuffer extra); // [extra]
VkCommandBuffer vk_upload_cmd(void); // [extra]
uint64_t vk_pending_seq(void); // [extra]
void vk_pace_frame(void); // [extra]
void vk_gpuprof_report(void); // [extra]
void vk_dl_note_upload(void *img, uint32_t w, uint32_t h, uint32_t lv); // [extra]
void vk_devlost_dump(void); // [extra]
void vk_collect_dead(void); // [extra]

