// [extra]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "draw_internal.h"
#include "../../../renderer.h"

#define AC_MAX_SKIP_HASHES 16
#define AC_WATCH_MAX_LOGS  4000

void ct_draw(Prog *pr, const char *what) {
    LOG("[ct] %s fs %016llx rt", what, (unsigned long long)g_cur_fs_hash);
    for (int i = 0; i < st.rt.ncolor; i++) {
        if (st.rt.color[i]) LOG(" c%d=%p/%#x", i, (void *)st.rt.color[i], st.rt.color[i]->format);
    }
    if (st.rt.depth) {
        LOG(" d=%p/%#x%s", (void *)st.rt.depth, st.rt.depth->format, st.ds[0] && st.ds[1] ? "W" : "");
    }
    LOG(" cm %08x vp %.0f %.0f %.0f %.0f sc %d %d %d %d | tex",
        st.channel_mask, st.viewport[0][0], st.viewport[0][1], st.viewport[0][2],
        st.viewport[0][3], st.scissor[0][0], st.scissor[0][1], st.scissor[0][2], st.scissor[0][3]);
    ShaderInfo *infos[2] = {pr->vi, pr->fi};
    for (int sg = 0; sg < 2; sg++) {
        int ntex = infos[sg] ? infos[sg]->ntex : 0;
        for (int i = 0; i < ntex; i++) {
            NvnTexture *t = shader_texture_object(sg, &infos[sg]->tex[i]);
            if (t) LOG(" %s%p/%#x", sg ? "" : "v:", (void *)t, t->format);
        }
    }
    LOG("\n");
}

void debug_draw(Prog *pr, CmdRec *r, bool indexed) {
    LOG("[draw] %s inst %llu mode %llu count %llu prog vs=%llx fs=%llx (%016llx %016llx) | vs cb %#x fs cb %#x | rt %d",
        indexed ? "e" : "a", (unsigned long long)(indexed ? r->x[6] : r->x[4]), (unsigned long long)r->x[0],
        (unsigned long long)r->x[2], (unsigned long long)st.program->stages[0].code_addr,
        (unsigned long long)st.program->stages[1].code_addr, (unsigned long long)pr->stage_hash[0],
        (unsigned long long)pr->stage_hash[1], pr->vi->cbuf_mask, pr->fi->cbuf_mask, st.rt.ncolor);
    for (int i = 0; i < st.rt.ncolor; i++) {
        if (st.rt.color[i]) LOG(" %ux%u/f%#x", st.rt.color[i]->width, st.rt.color[i]->height, st.rt.color[i]->format);
    }
    if (st.rt.depth) LOG(" d%#x %p", st.rt.depth->format, (void *)st.rt.depth);
    LOG(" | vp %.0f %.0f %.0f %.0f | attrs %#x blend %#x ds %02x%02x%02x poly %02x%02x chmask %08x\n",
        st.viewport[0][0], st.viewport[0][1], st.viewport[0][2], st.viewport[0][3], pr->vi->attr_mask,
        st.color_state[0], st.ds[0], st.ds[1], st.ds[2], st.poly[0], st.poly[1], st.channel_mask);
    static int dbg_data = -1;
    if (dbg_data < 0) dbg_data = getenv("LARECOMP_DEBUG_DATA") != NULL;
    if (dbg_data) {
        for (int s = 0; s < 2; s++) {
            if (!st.vbuf[s].addr) continue;
            const float *v = (const float *)(uintptr_t)st.vbuf[s].addr;
            LOG("[draw]   vbuf%d %llx size %llu:", s, (unsigned long long)st.vbuf[s].addr, (unsigned long long)st.vbuf[s].size);
            for (int k = 0; k < 16; k++) LOG(" %.4g", v[k]);
            LOG("\n");
        }
        uint32_t masks[2] = {pr->vi->cbuf_mask, pr->fi->cbuf_mask};
        const char *labels[2] = {"vcb", "fcb"};
        for (int s = 0; s < 2; s++) {
            for (int c = 0; c < 18; c++) {
                if (!((masks[s] >> c) & 1)) continue;
                int idx = c >= 3 ? c - 3 : c;
                Range cb_range = st.ubuf[s][idx].addr ? st.ubuf[s][idx] : st.ubuf[s][c];
                if (!cb_range.addr) continue;
                const float *u = (const float *)(uintptr_t)cb_range.addr;
                LOG("[draw]   %s%d(idx%d) %llx size %u:", labels[s], c, idx, (unsigned long long)cb_range.addr, (unsigned)cb_range.size);
                for (int k = 0; k < 96 && k * 4 < (int)cb_range.size; k++) LOG(" %.6g", u[k]);
                LOG("\n");
            }
        }
    }
    for (int i = 0; i < pr->fi->ntex && i < AC_MAX_TEX_SLOTS; i++) {
        NvnTexture *t = shader_texture_object(1, &pr->fi->tex[i]);
        const TexBackend *b = t ? (const TexBackend *)t->backend : NULL;
        if (!t) { LOG("[draw]   tex%d: none\n", i); continue; }
        LOG("[draw]   tex%d: %p %ux%u f%#x lv %u -> %ux%u scale %.4f %.4f%s\n",
            i, (void *)t, t->width, t->height, t->format, b ? b->levels : 0, b ? b->width : 0,
            b ? b->height : 0, b ? b->sx : 0, b ? b->sy : 0, b && b->scalable ? " scalable" : "");
    }
}

bool fs_skip_or_watch(Prog *pr, CmdRec *r) {
    static uint64_t skip[AC_MAX_SKIP_HASHES];
    static int nskip = -1;
    if (nskip < 0) {
        nskip = 0;
        for (const char *p = getenv("LARECOMP_SKIP_FSHASH"); p && *p && nskip < AC_MAX_SKIP_HASHES;) {
            skip[nskip++] = strtoull(p, (char **)&p, 16);
            while (*p == ',') p++;
        }
    }
    uint64_t short_hash = pr->fs_short_hash;
    uint64_t fs_hash = pr->stage_hash[1];
    for (int i = 0; i < nskip; i++) {
        if (short_hash == skip[i] || fs_hash == skip[i]) return true;
    }
    static int log_fshash = -1;
    if (log_fshash < 0) log_fshash = getenv("LARECOMP_LOG_FSHASH") != NULL;
    if (g_debug_frame && log_fshash) {
        uint64_t fs = (st.program->stage_mask & 2) ? st.program->stages[1].code_addr : 0;
        LOG("[fshash] fs=%llx hash %016llx\n", (unsigned long long)fs, (unsigned long long)short_hash);
    }
    g_cur_fs_hash = short_hash;
    if (st.ubuf[0][5].addr) {
        extern void widescreen_scene_ubo(const float *u, uint64_t size);
        widescreen_scene_ubo((const float *)(uintptr_t)st.ubuf[0][5].addr, st.ubuf[0][5].size);
    }
    static uint64_t watch = 1;
    static int nwatch;
    static const char *watch_dbg = (const char *)1;
    if (watch == 1) {
        const char *e = getenv("LARECOMP_WATCH_FS");
        watch = e ? strtoull(e, NULL, 16) : 0;
    }
    if (watch && nwatch < AC_WATCH_MAX_LOGS && fs_hash == watch) {
        nwatch++;
        if (watch_dbg == (const char *)1) watch_dbg = getenv("LARECOMP_WATCH_DEBUG");
        if (watch_dbg && nwatch == atoi(watch_dbg)) {
            extern volatile int g_debug_frame_request;
            g_debug_frame_request = 1;
        }
        LOG("[watch] alpha test in key %u, shader %s\n", pr->fkey.alpha_func,
            pr->fi && pr->fi->msl && strstr(pr->fi->msl, ")) discard;") ? "tests" : "does not test");
        LOG("[watch] frame %llu count %llu blend %#x [%u %u %u %u %u %u %u %u] a2c %u alpha test %u ref %g ds %02x%02x%02x |",
            (unsigned long long)renderer_present_count(), (unsigned long long)r->x[2], st.color_state[0], st.blend[0][0],
            st.blend[0][1], st.blend[0][2], st.blend[0][3], st.blend[0][4], st.blend[0][5], st.blend[0][6], st.blend[0][7],
            st.ms[0xC], st.color_state[2], st.alpha_ref, st.ds[0], st.ds[1], st.ds[2]);
        for (int s = 0; s < 2; s++) {
            uint32_t mask = s ? pr->fi->cbuf_mask : pr->vi->cbuf_mask;
            for (int k = 3; k < 21; k++) {
                if (!((mask >> k) & 1)) continue;
                Range u = st.ubuf[s][k - 3];
                LOG(" | %s cb%d", s ? "fs" : "vs", k);
                if (!u.addr) { LOG(" unbound"); continue; }
                for (int i = 0; i < 24 && i * 4 < (int)u.size; i++) LOG(" %.4g", ((const float *)(uintptr_t)u.addr)[i]);
            }
        }
        LOG("\n");
    }
    return false;
}
// [extra]
