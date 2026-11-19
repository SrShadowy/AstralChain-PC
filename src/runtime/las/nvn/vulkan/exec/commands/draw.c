// [extra]
#include <math.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../../../settings.h"
#include "draw_internal.h"
#include "../../../renderer.h"
#include "hooks/gpu/draw_classify.h"
#include "hooks/gpu/vtx_debug_extra.h"
#include "hooks/gpu/aspect_extra.h"

#define NVN_ALPHA_OP_LIMIT 8u
#define AC_UI_VIRTUAL_W    1280.0f
#define AC_UI_VIRTUAL_H    720.0f
#define AC_ASPECT_16_9     (16.0f / 9.0f)

uint64_t g_cur_fs_hash;

static bool topology(uint32_t mode, VkPrimitiveTopology *t, uint8_t *cls) {
    switch (mode) {
    case NVN_PRIM_POINTS: *t = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; *cls = 0; return true;
    case NVN_PRIM_LINES: *t = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; *cls = 1; return true;
    case NVN_PRIM_LINE_STRIP: *t = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; *cls = 1; return true;
    case NVN_PRIM_TRIANGLES: *t = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; *cls = 2; return true;
    case NVN_PRIM_TRIANGLE_STRIP: *t = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; *cls = 2; return true;
    default: *t = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; *cls = 2; return false;
    }
}

static uint16_t half_bits(float f) {
    uint32_t x;
    memcpy(&x, &f, 4);
    uint32_t sign = (x >> 16) & 0x8000u, m = x & 0x7FFFFFu;
    int e = (int)((x >> 23) & 0xFF) - 112;
    if (((x >> 23) & 0xFF) == 0xFF) return (uint16_t)(sign | 0x7C00u | (m ? 0x200u : 0));
    if (e >= 31) return (uint16_t)(sign | 0x7C00u);
    if (e <= 0) {
        if (e < -10) return (uint16_t)sign;
        m |= 0x800000u;
        uint32_t sh = (uint32_t)(14 - e), h = m >> sh;
        return (uint16_t)(sign | (h + ((m >> (sh - 1)) & 1)));
    }
    return (uint16_t)((sign | ((uint32_t)e << 10) | (m >> 13)) + ((m >> 12) & 1));
}

static Prog *resolve_draw_program(void) {
    static uint64_t cached_gen;
    static uint32_t cached_origin = ~0u, cached_depth = ~0u;
    static Prog *cached_pr;
    uint32_t origin = nvn_window_origin();
    uint32_t depth_mode = nvn_depth_mode();
    if (cached_pr && cached_gen == st.prog_gen && cached_origin == origin && cached_depth == depth_mode &&
        atomic_load(&cached_pr->state) == 1) {
        return cached_pr;
    }
    ShaderKey vkey = {0}, fkey = {0};
    vkey.glsl = fkey.glsl = 1;
    vkey.flip_y = fkey.flip_y = origin == 0;
    vkey.depth_minus_one = depth_mode == 0;
    for (int i = 0; i < st.nattrib; i++) vkey.attr_kind[i] = (uint8_t)attr_kind(st.attrib[i] & 0xFF);
    for (int i = 0; i < st.rt.ncolor; i++) {
        if (st.rt.color[i]) fkey.rt_kind[i] = (uint8_t)rt_kind(st.rt.color[i]->format);
    }
    if (st.color_state[2] && st.color_state[2] < NVN_ALPHA_OP_LIMIT) {
        fkey.alpha_func = st.color_state[2];
        fkey.alpha_ref = half_bits(st.alpha_ref);
    }
    Prog *pr = get_program(st.program, &vkey, &fkey);
    if (pr) wait_built(&pr->state);
    if (!pr || atomic_load(&pr->state) != 1) {
        cached_pr = NULL;
        return NULL;
    }
    cached_pr = pr;
    cached_gen = st.prog_gen;
    cached_origin = origin;
    cached_depth = depth_mode;
    return pr;
}

static Pipe *resolve_draw_pipeline(Prog *pr, uint8_t cls) {
    static uint64_t cached_pipe_gen, cached_pass_gen;
    static uint8_t cached_cls = 0xFF;
    static Prog *cached_pr;
    static Pipe *cached_pp;
    if (cached_pp && cached_pipe_gen == st.pipe_gen && cached_pass_gen == pass_gen &&
        cached_cls == cls && cached_pr == pr && atomic_load(&cached_pp->state) == 1) {
        return cached_pp;
    }
    PipeKey k;
    memset(&k, 0, sizeof k);
    k.prog = pr;
    k.ncolor = pass_ncolor;
    memcpy(k.color, pass_color, sizeof k.color);
    k.depth = pass_depth;
    k.stencil = pass_stencil;
    k.samples = pass_samples;
    for (int i = 0; i < 32; i++) {
        if (!(pr->vi->attr_mask & (1u << i))) continue;
        k.attr_mask |= 1u << i;
        uint32_t a = i < st.nattrib ? st.attrib[i] : 0x2E;
        uint32_t fmt = a & 0xFF, stream = (a >> 8) & 0x1F, off = a >> 13;
        if (i >= st.nattrib || fmt == 0 || stream >= 16) {
            k.attr_fmt[i] = VK_FORMAT_R32G32B32A32_SFLOAT;
            k.attr_bind[i] = ZERO_BINDING;
            k.used_streams |= 1u << ZERO_BINDING;
            continue;
        }
        k.attr_fmt[i] = vertex_format_supported(vertex_format(fmt));
        k.attr_off[i] = off;
        k.attr_bind[i] = stream;
        k.used_streams |= 1u << stream;
    }
    for (int s = 0; s < 16; s++) {
        if (!(k.used_streams & (1u << s))) continue;
        k.stride[s] = s < st.nstream ? st.stream[s][0] : 0;
        k.divisor[s] = s < st.nstream && k.stride[s] ? st.stream[s][1] : 0;
        if (k.divisor[s] > 1 && !g_vk.divisor) k.divisor[s] = 1;
    }
    for (uint32_t i = 0; i < k.ncolor && i < 8; i++) {
        if (!st.rt.color[i] || k.color[i] == VK_FORMAT_UNDEFINED) continue;
        k.write_mask[i] = ((pr->fi->out_mask >> (i * 4)) & 0xF) ? (st.channel_mask >> (i * 4)) & 0xF : 0;
        if (((st.color_state[0] >> i) & 1) && !rt_kind(st.rt.color[i]->format)) {
            k.blend_on |= (uint8_t)(1u << i);
            memcpy(k.blend[i], st.blend[i], 8);
        }
    }
    k.a2c = st.ms[0xC] != 0;
    k.topo_class = cls;
    k.poly_fill = st.poly[2] != 1;
    Pipe *pp = pipe_lookup(&k, pr);
    if (!pp || atomic_load(&pp->state) != 1) {
        cached_pp = NULL;
        return NULL;
    }
    cached_pp = pp;
    cached_pr = pr;
    cached_pipe_gen = st.pipe_gen;
    cached_pass_gen = pass_gen;
    cached_cls = cls;
    return pp;
}

static bool apply_depth_stencil_state(VkCommandBuffer cb, bool *out_equal_tol) {
    static int nodt = -1, tol = -1;
    if (nodt < 0) nodt = getenv("LARECOMP_NO_DEPTH_TEST") != NULL;
    if (tol < 0) tol = getenv("LARECOMP_VK_EQUAL_TOL") != NULL;
    bool dt = st.ds[0] && !nodt && pass_depth;
    if (ss_need(SS_DT, ss.dt == (VkBool32)dt)) { ss.dt = dt; vkCmdSetDepthTestEnable(cb, dt); }
    VkBool32 dw = dt && st.ds[1];
    if (ss_need(SS_DW, ss.dw == dw)) { ss.dw = dw; vkCmdSetDepthWriteEnable(cb, dw); }
    VkCompareOp dop = dt ? compare_fn(st.ds[2]) : VK_COMPARE_OP_ALWAYS;
    bool equal_tol = dop == VK_COMPARE_OP_EQUAL && tol;
    if (equal_tol) dop = VK_COMPARE_OP_LESS_OR_EQUAL;
    if (ss_need(SS_DOP, ss.dop == dop)) { ss.dop = dop; vkCmdSetDepthCompareOp(cb, dop); }
    bool sten = st.ds[3] && pass_stencil;
    if (ss_need(SS_STEN, ss.sten == (VkBool32)sten)) { ss.sten = sten; vkCmdSetStencilTestEnable(cb, sten); }
    if (sten) {
        uint32_t ops = st.ds[5] | (st.ds[6] << 8) | (st.ds[7] << 16);
        for (int f = 0; f < 2; f++) {
            uint32_t o = f ? ops >> 12 : ops;
            VkStencilFaceFlags face = f ? VK_STENCIL_FACE_BACK_BIT : VK_STENCIL_FACE_FRONT_BIT;
            uint32_t v[7] = {
                stencil_op(o & 0xF), stencil_op((o >> 8) & 0xF), stencil_op((o >> 4) & 0xF),
                compare_fn(f ? st.ds[4] >> 4 : st.ds[4] & 0xF),
                st.stencil_vmask[f] & 0xFF, st.stencil_mask[f] & 0xFF, st.stencil_ref[f]
            };
            if (!ss_need(f ? SS_SFACE1 : SS_SFACE0, !memcmp(v, ss.sface[f], sizeof v))) continue;
            memcpy(ss.sface[f], v, sizeof v);
            vkCmdSetStencilOp(cb, face, (VkStencilOp)v[0], (VkStencilOp)v[1], (VkStencilOp)v[2], (VkCompareOp)v[3]);
            vkCmdSetStencilCompareMask(cb, face, v[4]);
            vkCmdSetStencilWriteMask(cb, face, v[5]);
            vkCmdSetStencilReference(cb, face, v[6]);
        }
    }
    if (ss_need(SS_BC, !memcmp(ss.bc, st.blend_color, sizeof ss.bc))) {
        memcpy(ss.bc, st.blend_color, sizeof ss.bc);
        vkCmdSetBlendConstants(cb, st.blend_color);
    }
    *out_equal_tol = equal_tol;
    return dt;
}

static void apply_raster_state(VkCommandBuffer cb, uint32_t mode, VkPrimitiveTopology topo, bool native, bool flip_y, bool equal_tol) {
    uint8_t cull = st.poly[0];
    VkCullModeFlags cm = cull == 1 ? VK_CULL_MODE_FRONT_BIT : cull == 2 ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
    if (ss_need(SS_CULL, ss.cull == cm)) { ss.cull = cm; vkCmdSetCullMode(cb, cm); }
    static int flipw = -1;
    if (flipw < 0) flipw = getenv("LARECOMP_VK_FLIP_WINDING") != NULL;
    bool front_ccw = ((st.poly[1] == 1) != flip_y) != (flipw != 0);
    VkFrontFace ff = front_ccw ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
    if (ss_need(SS_FF, ss.ff == ff)) { ss.ff = ff; vkCmdSetFrontFace(cb, ff); }
    VkPrimitiveTopology tp = native ? topo : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    if (ss_need(SS_TOPO, ss.topo == tp)) { ss.topo = tp; vkCmdSetPrimitiveTopology(cb, tp); }
    VkBool32 pre = st.prim_restart && native && (mode == NVN_PRIM_LINE_STRIP || mode == NVN_PRIM_TRIANGLE_STRIP);
    if (ss_need(SS_PRE, ss.pre == pre)) { ss.pre = pre; vkCmdSetPrimitiveRestartEnable(cb, pre); }
    static int bias_off = -1;
    if (bias_off < 0) bias_off = getenv("LARECOMP_NO_DEPTH_BIAS") != NULL;
    float f[3] = {0, 0, 0};
    bool on = (st.poly[3] & 4) && !bias_off;
    if (on) memcpy(f, st.poly_offset, sizeof f);
    if (equal_tol) { f[1] -= 8; on = true; }
    if (ss_need(SS_DBE, ss.dbe == (VkBool32)on)) { ss.dbe = on; vkCmdSetDepthBiasEnable(cb, on); }
    if (ss_need(SS_DB, !memcmp(ss.db, f, sizeof f))) {
        memcpy(ss.db, f, sizeof f);
        vkCmdSetDepthBias(cb, f[1], f[2], f[0]);
    }
}

static void update_draw_viewport(Prog *pr, CmdRec *r, const PipeKey *pk) {
    float *vp = st.viewport[0];
    float zn = st.depth_range[0][0], zf = st.depth_range[0][1];
    vp_sx = pass_sx; vp_sy = pass_sy; vp_ox = 0; vp_oy = 0;
    if (fabsf(pass_sx - pass_sy) > 0.01f * pass_sy && st.rt.ncolor && renderer_texture_presented(st.rt.color[0])) {
        bool reads_scene = false;
        for (int i = 0; i < pr->fi->ntex && i < AC_MAX_TEX_SLOTS; i++) {
            if ((tex_sc[1][i][0] != 1 || tex_sc[1][i][1] != 1) &&
                tex_wh[1][i][0] == st.rt.color[0]->width && tex_wh[1][i][1] == st.rt.color[0]->height) {
                reads_scene = true;
            }
        }
        bool fullscreen_quad = r->x[2] <= 6 && vp[0] == 0 && vp[1] == 0 &&
                               vp[2] >= st.rt.color[0]->width && vp[3] >= st.rt.color[0]->height;
        bool fullscreen_fill = pr->fi->ntex == 0 && fullscreen_quad;
        if (fullscreen_quad && !reads_scene && pr->fi->ntex == 1 && (tex_sc[1][0][0] != 1 || tex_sc[1][0][1] != 1)) {
            reads_scene = true;
        }
        static uint64_t wide_frame = ~0ull;
        if (reads_scene) wide_frame = renderer_present_count();
        bool fade_fill = false;
        if (fullscreen_fill && st.ubuf[1][0].addr && st.ubuf[1][0].size >= 12) {
            const float *c = (const float *)(uintptr_t)st.ubuf[1][0].addr;
            bool black = c[0] < 0.01f && c[1] < 0.01f && c[2] < 0.01f;
            bool white = c[0] > 0.99f && c[1] > 0.99f && c[2] > 0.99f;
            fade_fill = black || white;
        }
        bool composite = reads_scene || fade_fill || (fullscreen_fill && wide_frame == renderer_present_count());
        if (!composite) {
            wide_frame = ~0ull;
            int pw, ph;
            pass_size(&pw, &ph);
            float u = fminf(pass_sx, pass_sy);
            vp_sx = vp_sy = u;
            vp_ox = (pw - st.rt.color[0]->width * u) * 0.5f;
            vp_oy = (ph - st.rt.color[0]->height * u) * 0.5f;
        }
    }
    bool ui_box = false;
    if (st.rt.ncolor && st.rt.color[0] &&
        (uint64_t)st.rt.color[0]->width * 9 > (uint64_t)st.rt.color[0]->height * 16 * 101 / 100 &&
        pr->stage_hash[0] == AC_UI_BASE_VS_HASH && st.ubuf[0][0].addr && st.ubuf[0][0].size >= 40 * 4) {
        const float *p = (const float *)(uintptr_t)st.ubuf[0][0].addr;
        ui_box = fabsf(p[32] - 2.0f / AC_UI_VIRTUAL_W) < 1e-6f && p[33] == 0 && p[35] == -1.0f &&
                 fabsf(p[37] + 2.0f / AC_UI_VIRTUAL_H) < 1e-6f && p[39] == 1.0f;
        if (ui_box && r->x[2] <= 6 && pk->stride[0] >= 16 && st.vbuf[0].addr && st.vbuf[0].size >= 4 * pk->stride[0] &&
            st.ubuf[0][1].addr && st.ubuf[0][1].size >= 16) {
            const float *m = (const float *)(uintptr_t)st.ubuf[0][1].addr;
            float lo = 1e30f, hi = -1e30f;
            for (int i = 0; i < 4; i++) {
                const float *v = (const float *)(uintptr_t)(st.vbuf[0].addr + (uint64_t)i * pk->stride[0]);
                float x = m[0] * v[0] + m[1] * v[1] + m[2] * v[2] + m[3] * v[3];
                lo = fminf(lo, x);
                hi = fmaxf(hi, x);
            }
            if (lo <= 0.5f && hi >= 1279.5f) ui_box = false;
        }
    }
    float vx = vp[0] * vp_sx + vp_ox, vw = vp[2] * vp_sx;
    if (ui_box && extra_hud_area_centered()) {
        float bw = vp[3] * vp_sy * AC_ASPECT_16_9;
        vx += (vw - bw) * 0.5f;
        vw = bw;
    }
    set_viewport(vx, vp[1] * vp_sy + vp_oy, vw, vp[3] * vp_sy, zn, zf);
    if (g_debug_frame) {
        VkRect2D sr = scissor_rect();
        LOG("[draw]   vp %.2f %.2f %.2f %.2f (fs hash %016llx) scissor %d %d %u %u | guest sc %d %d %d %d | pass %ux%u s %.4f %.4f\n",
            vx, vp[1] * vp_sy + vp_oy, vw, vp[3] * vp_sy, (unsigned long long)g_cur_fs_hash,
            sr.offset.x, sr.offset.y, sr.extent.width, sr.extent.height,
            st.scissor[0][0], st.scissor[0][1], st.scissor[0][2], st.scissor[0][3], pass_w, pass_h, vp_sx, vp_sy);
    }
}

static void draw_(CmdRec *r, bool indexed) {
    uint64_t prof_tp = prof ? SDL_GetTicksNS() : 0;
    if (!st.program) { SKIPPED(); return; }
    Prog *pr = resolve_draw_program();
    if (!pr) { SKIPPED(); return; }
    if (fs_skip_or_watch(pr, r)) { SKIPPED(); return; }
    if (g_debug_frame) debug_draw(pr, r, indexed);
    if (ct_on()) ct_draw(pr, "draw");

    alias_draw_check();
    resolve_feedback(pr);
    snap_reason = "draw-depthwrite";
    if (st.rt.depth && st.ds[0] && st.ds[1]) snapshots_invalidate(st.rt.depth);
    snap_reason = "draw-colorwrite";
    for (int i = 0; i < st.rt.ncolor; i++) {
        if (st.rt.color[i] && ((st.channel_mask >> (i * 4)) & 0xF)) snapshots_invalidate(st.rt.color[i]);
    }
    static int dbg_each = -1;
    if (dbg_each < 0) dbg_each = getenv("LARECOMP_DEBUG_EACH") != NULL;
    if (g_debug_frame && dbg_each && in_pass) {
        LOG("[draw]   (state before this draw: pass image %d)\n", debug_pass_no);
        vk_end_pass();
    }
    begin_pass();
    VkCommandBuffer cb = vk_cmd();

    uint32_t mode = (uint32_t)r->x[0];
    VkPrimitiveTopology topo;
    uint8_t cls;
    bool native = topology(mode, &topo, &cls);
    if (!native && mode != NVN_PRIM_TRIANGLE_FAN && mode != NVN_PRIM_QUADS && mode != NVN_PRIM_POLYGON) {
        SKIPPED();
        return;
    }

    uint64_t prof_tk = prof ? SDL_GetTicksNS() : 0;
    if (prof) prof_pre += prof_tk - prof_tp;

    Pipe *pp = resolve_draw_pipeline(pr, cls);
    if (!pp) { SKIPPED(); return; }
    ss_enabled();
    VkPipeline use = (VkPipeline)(uintptr_t)atomic_load(&pp->opt);
    if (!use) use = pp->pipe;
    if (ss_need(SS_PIPE, ss.pipe == use)) { ss.pipe = use; vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, use); }

    uint64_t prof_td = prof ? SDL_GetTicksNS() : 0;
    if (prof) prof_key += prof_td - prof_tk;

    bool equal_tol = false;
    bool dt = apply_depth_stencil_state(cb, &equal_tol);
    apply_raster_state(cb, mode, topo, native, pr->vkey.flip_y, equal_tol);
    if (prof) prof_dyn += SDL_GetTicksNS() - prof_td;

    PROF_T0();
    VkDescriptorSet vset = stage_set(pr, pr->set_layout[0], 0, 0, pr->vi, NULL, NULL, 0, 0, 0, NULL);
    PROF_ADD(prof_sets);

    VkDescriptorImageInfo fii[AC_MAX_TEX_SLOTS];
    uint64_t prof_tx = prof ? SDL_GetTicksNS() : 0;
    for (int i = 0; i < pr->fi->ntex && i < AC_MAX_TEX_SLOTS; i++) {
        float sc[2]; uint32_t wh[2];
        texture_info(1, pr->fi, i, &fii[i], sc, wh);
        tex_sc[1][i][0] = sc[0]; tex_sc[1][i][1] = sc[1];
        tex_wh[1][i][0] = wh[0]; tex_wh[1][i][1] = wh[1];
    }
    if (prof) prof_tex += SDL_GetTicksNS() - prof_tx;

    update_draw_viewport(pr, r, &pp->k);
    apply_scissor();
    float rs0[4] = {vp_sx, vp_sy, vp_ox, vp_oy};
    uint64_t prof_t1 = prof ? SDL_GetTicksNS() : 0;
    VkDescriptorSet fset = stage_set(pr, pr->set_layout[1], 1, 1, pr->fi, rs0, NULL, 0, 0, 0, fii);
    if (prof) prof_sets += SDL_GetTicksNS() - prof_t1;
    if (!ss_on || ss.bound_layout != pr->layout) { // [extra]
        VkDescriptorSet sets[2] = {vset, fset};
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pr->layout, 0, 2, sets, 0, NULL);
        ss.bound_layout = pr->layout;
        ss.bound_sets[0] = vset;
        ss.bound_sets[1] = fset;
    } else if (ss.bound_sets[0] != vset && ss.bound_sets[1] != fset) {
        VkDescriptorSet sets[2] = {vset, fset};
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pr->layout, 0, 2, sets, 0, NULL);
        ss.bound_sets[0] = vset;
        ss.bound_sets[1] = fset;
    } else if (ss.bound_sets[0] != vset) {
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pr->layout, 0, 1, &vset, 0, NULL);
        ss.bound_sets[0] = vset;
    } else if (ss.bound_sets[1] != fset) {
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pr->layout, 1, 1, &fset, 0, NULL);
        ss.bound_sets[1] = fset;
    } // [extra]
    if (pr->vi->uses_gmem_store) vc_mark_sbufs(0);
    if (pr->fi->uses_gmem_store) vc_mark_sbufs(1);

    uint64_t prof_t2 = prof ? SDL_GetTicksNS() : 0;
    DrawGeomInfo geom;
    bind_draw_indices(cb, pr, r, indexed, native, dt, mode, &geom);
    if ((extra_vtx_debug_enabled() || extra_vtx_guard_enabled()) && st.vbuf[0].addr) {
        resolve_deferred_index_scan(&geom);
        extra_vtx_guard_stream((const void *)(uintptr_t)st.vbuf[0].addr, geom.max_index,
                               (uint32_t)pp->k.stride[0], (uint64_t)st.program);
    }
    bind_vertex_streams(cb, &pp->k, &geom);
    if (prof) prof_vtx += SDL_GetTicksNS() - prof_t2;

    extra_vtx_guard_cbuf((const void *)(uintptr_t)st.ubuf[1][0].addr, (size_t)st.ubuf[1][0].size, (uint64_t)st.program);

    static int per_draw = -1;
    if (g_gpuprof_frame) {
        if (per_draw < 0) per_draw = getenv("LARECOMP_GPUPROF_DRAWS") != NULL;
        if (per_draw) {
            gp_mark(cb, "draw vs %llx fs %llx fs hash %016llx count %u inst %u",
                    (unsigned long long)st.program->stages[0].code_addr,
                    (unsigned long long)st.program->stages[1].code_addr,
                    (unsigned long long)g_cur_fs_hash,
                    geom.use_index ? geom.icount : geom.count, geom.inst);
        }
    }
    oq_draw(cb, false);
    if (!geom.use_index) {
        vkCmdDraw(cb, geom.count, geom.inst, geom.first, geom.base_inst);
    } else {
        vkCmdDrawIndexed(cb, geom.icount, geom.inst, 0, geom.base_vertex, geom.base_inst);
    }
    oq_draw(cb, true);
    stat_draws++; vk_draws_total++; cum_draws++;
    gp_record_draw(g_cur_fs_hash);
}

void draw(CmdRec *r, bool indexed) {
    PROF_T0();
    draw_(r, indexed);
    PROF_ADD(prof_draw);
}
// [extra]
