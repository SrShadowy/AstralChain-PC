// [extra]
#include <stddef.h>
#include <stdint.h>

#include "draw_internal.h"
#include "hooks/gpu/draw_classify.h"

#define NVN_INDEX_U8          0u
#define NVN_INDEX_U16         1u
#define NVN_INDEX_RESTART_U8  0xFFu
#define NVN_INDEX_RESTART_U16 0xFFFFu
#define NVN_INDEX_RESTART_U32 0xFFFFFFFFu

#define AC_DIRECT_COPY_LIMIT  (256u << 10)
#define AC_STREAM_PAD_BYTES   256u

static inline uint32_t read_index_at(const void *indices, uint32_t itype, uint32_t first, uint32_t i) {
    if (!indices) return first + i;
    if (itype == NVN_INDEX_U8) return ((const uint8_t *)indices)[i];
    if (itype == NVN_INDEX_U16) return ((const uint16_t *)indices)[i];
    return ((const uint32_t *)indices)[i];
}

static uint32_t scan_max_index(const void *src, uint32_t isz, uint32_t count, uint32_t restart, uint32_t cur_max) {
    if (isz == 2) {
        const uint16_t *s = src;
        uint16_t r16 = (uint16_t)restart;
        for (uint32_t i = 0; i < count; i++) {
            if (s[i] > cur_max && s[i] != r16) cur_max = s[i];
        }
        return cur_max;
    }
    const uint32_t *s = src;
    for (uint32_t i = 0; i < count; i++) {
        if (s[i] > cur_max && s[i] != restart) cur_max = s[i];
    }
    return cur_max;
}

static VkSpan expand_indices_transient(uint32_t mode, uint32_t count, const void *indices, uint32_t itype,
                                       uint32_t first, uint32_t *out_count, uint32_t *out_max) {
    uint32_t tris = mode == NVN_PRIM_QUADS ? (count / 4) * 2 : (count >= 3 ? count - 2 : 0);
    uint32_t icount = tris * 3;
    VkSpan span = vk_transient((uint64_t)icount * 4 + 4, 4);
    uint32_t *out = span.ptr;
    uint32_t n = 0;
    uint32_t max_idx = out_max ? *out_max : 0;
    if (mode == NVN_PRIM_QUADS) {
        for (uint32_t q = 0; q + 3 < count; q += 4) {
            uint32_t i0 = read_index_at(indices, itype, first, q);
            uint32_t i1 = read_index_at(indices, itype, first, q + 1);
            uint32_t i2 = read_index_at(indices, itype, first, q + 2);
            uint32_t i3 = read_index_at(indices, itype, first, q + 3);
            out[n++] = i0; out[n++] = i1; out[n++] = i2;
            out[n++] = i0; out[n++] = i2; out[n++] = i3;
            if (out_max) {
                if (i0 > max_idx) max_idx = i0;
                if (i1 > max_idx) max_idx = i1;
                if (i2 > max_idx) max_idx = i2;
                if (i3 > max_idx) max_idx = i3;
            }
        }
    } else {
        uint32_t i0 = count ? read_index_at(indices, itype, first, 0) : 0;
        if (out_max && count && i0 > max_idx) max_idx = i0;
        for (uint32_t i = 1; i + 1 < count; i++) {
            uint32_t i1 = read_index_at(indices, itype, first, i);
            uint32_t i2 = read_index_at(indices, itype, first, i + 1);
            out[n++] = i0; out[n++] = i1; out[n++] = i2;
            if (out_max) {
                if (i1 > max_idx) max_idx = i1;
                if (i2 > max_idx) max_idx = i2;
            }
        }
    }
    *out_count = n;
    if (out_max) *out_max = max_idx;
    return span;
}

static inline uint64_t calc_stream_need(uint64_t stride, uint32_t divisor, uint32_t base_inst, uint32_t inst, uint32_t max_index) {
    if (!stride) return AC_STREAM_PAD_BYTES;
    if (divisor) return ((uint64_t)base_inst + inst + divisor - 1) / divisor * stride + AC_STREAM_PAD_BYTES;
    return (uint64_t)max_index * stride + AC_STREAM_PAD_BYTES;
}

void bind_draw_indices(VkCommandBuffer cb, Prog *pr, CmdRec *r, bool indexed, bool native, bool dt, uint32_t mode, DrawGeomInfo *g) {
    g->count = (uint32_t)r->x[2];
    g->first = 0;
    g->base_vertex = 0;
    g->max_index = 0;
    g->icount = 0;
    g->use_index = indexed || !native;
    g->scan_src = NULL;
    g->scan_isz = 0;
    g->scan_restart = 0;

    uint32_t inst_count = indexed ? (uint32_t)r->x[6] : (uint32_t)r->x[4];
    g->small = extra_should_copy_draw(pr->stage_hash[0], g_cur_fs_hash, dt, g->count, inst_count);

    VkSpan ib = {0};
    VkIndexType itype_vk = VK_INDEX_TYPE_UINT32;
    if (!indexed) {
        g->first = (uint32_t)r->x[1];
        g->base_inst = (uint32_t)r->x[3];
        g->inst = inst_count ? inst_count : 1;
        g->max_index = g->first + g->count;
        if (!native) ib = expand_indices_transient(mode, g->count, NULL, 0, g->first, &g->icount, NULL);
    } else {
        uint32_t itype = (uint32_t)r->x[1];
        uint64_t iaddr = r->x[3];
        g->base_vertex = (int32_t)r->x[4];
        g->base_inst = (uint32_t)r->x[5];
        g->inst = inst_count ? inst_count : 1;
        const void *src = (const void *)(uintptr_t)iaddr;
        uint32_t restart = st.prim_restart ? st.restart_index : NVN_INDEX_RESTART_U32;
        if (native && itype != NVN_INDEX_U8) {
            uint32_t isz = itype == NVN_INDEX_U16 ? 2 : 4;
            itype_vk = itype == NVN_INDEX_U16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
            g->icount = g->count;
            if (!g->small && iaddr % isz == 0 && guest_direct(iaddr, (uint64_t)g->count * isz, &ib.buf, &ib.off)) {
                g->scan_src = src;
                g->scan_isz = isz;
                g->scan_restart = restart;
            } else {
                ib = vk_transient_copy(src, (uint64_t)g->count * isz + 4, 4);
                g->max_index = scan_max_index(src, isz, g->count, restart, 0);
            }
        } else if (native) {
            ib = vk_transient((uint64_t)g->count * 2 + 4, 4);
            uint16_t *w = ib.ptr;
            const uint8_t *s = src;
            for (uint32_t i = 0; i < g->count; i++) {
                w[i] = (s[i] == NVN_INDEX_RESTART_U8 && st.prim_restart) ? NVN_INDEX_RESTART_U16 : s[i];
                if (s[i] > g->max_index && w[i] != NVN_INDEX_RESTART_U16) g->max_index = s[i];
            }
            itype_vk = VK_INDEX_TYPE_UINT16;
            g->icount = g->count;
        } else {
            ib = expand_indices_transient(mode, g->count, src, itype, 0, &g->icount, &g->max_index);
        }
        if (!g->scan_src) {
            g->max_index += 1;
            if (g->base_vertex > 0) g->max_index += (uint32_t)g->base_vertex;
        }
    }
    if (g->use_index && ss_need(SS_IB, ss.ib == ib.buf && ss.io == ib.off && ss.it == itype_vk)) {
        ss.ib = ib.buf; ss.io = ib.off; ss.it = itype_vk;
        vkCmdBindIndexBuffer(cb, ib.buf, ib.off, itype_vk);
    }
}

void resolve_deferred_index_scan(DrawGeomInfo *g) {
    if (!g->scan_src) return;
    g->max_index = scan_max_index(g->scan_src, g->scan_isz, g->icount, g->scan_restart, g->max_index);
    g->max_index += 1;
    if (g->base_vertex > 0) g->max_index += (uint32_t)g->base_vertex;
    g->scan_src = NULL;
}

void bind_vertex_streams(VkCommandBuffer cb, const PipeKey *pk, DrawGeomInfo *g) {
    for (int s = 0; s < 17; s++) {
        if (!(pk->used_streams & (1u << s))) continue;
        VkSpan sp;
        if (s == ZERO_BINDING) {
            static const float zero[4] = {0, 0, 0, 1};
            sp = vk_transient_copy(zero, 16, 16);
        } else {
            Range v = st.vbuf[s];
            VkBuffer dbuf;
            VkDeviceSize doff;
            uint64_t stride = pk->stride[s];
            uint64_t need = calc_stream_need(stride, pk->divisor[s], g->base_inst, g->inst, g->max_index);
            bool direct = !g->small || need > AC_DIRECT_COPY_LIMIT;
            if (direct && v.addr && v.size && guest_direct(v.addr, v.size, &dbuf, &doff)) {
                if (ss_on && ((ss.vb_known >> s) & 1) && ss.vb[s] == dbuf && ss.vo[s] == doff) continue;
                ss.vb[s] = dbuf; ss.vo[s] = doff; ss.vb_known |= 1u << s;
                vkCmdBindVertexBuffers(cb, (uint32_t)s, 1, &dbuf, &doff);
                continue;
            }
            if (g->scan_src) {
                resolve_deferred_index_scan(g);
                need = calc_stream_need(stride, pk->divisor[s], g->base_inst, g->inst, g->max_index);
            }
            if (v.size && need > v.size) need = v.size;
            if (!v.addr) {
                static const uint8_t zero[AC_STREAM_PAD_BYTES];
                sp = vk_transient_copy(zero, AC_STREAM_PAD_BYTES, 16);
            } else {
                uint64_t copy_sz = (v.size && v.size <= AC_STREAM_COPY_CAP_BYTES) ? v.size : (need ? need : 16);
                sp = guest_copy_vtx(v.addr, copy_sz);
            }
        }
        VkDeviceSize off = sp.off;
        if (ss_on && ((ss.vb_known >> s) & 1) && ss.vb[s] == sp.buf && ss.vo[s] == off) continue;
        ss.vb[s] = sp.buf; ss.vo[s] = off; ss.vb_known |= 1u << s;
        vkCmdBindVertexBuffers(cb, (uint32_t)s, 1, &sp.buf, &off);
    }
}
// [extra]
