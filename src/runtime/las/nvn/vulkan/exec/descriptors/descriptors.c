#include <stdio.h> // [extra]
#include <stdlib.h>
#include <string.h>
#include "../../exec_internal.h"
#include "../../../renderer.h"

NvnTexture *shader_texture_object(int stage, ShaderTexture *st_) {
    int slot = ((int)st_->handle_offset - 0x20) / 8;
    uint64_t h = (slot >= 0 && slot < 32) ? st.tex[stage][slot] : 0;
    if (st_->is_image) {  // (Astral Chain) SUST: image handles follow the 32 texture handles (0x120 + 8 * slot)
        slot = ((int)st_->handle_offset - 0x120) / 8;
        h = (slot >= 0 && slot < 8 && stage < 6) ? st.img[stage][slot] : 0;
    }
    uint32_t tic = h & 0xFFFFFu;
    if (!h || !st.tpool || tic >= (uint32_t)st.tpool->count) return NULL;
    return ((NvnTexEntry *)st.tpool->entries)[tic].tex;
}

// ================================================================== resource binding // [extra]
#define ZERO_UBO_CACHE_BYTES 0x800u
#define TEX_MEMO_BUCKETS     2048u
#define DS_CACHE_BUCKETS     2048u
#define DS_CACHE_MAX_BIND    24

static VkSpan zero_span_cached(uint64_t size) {
    static const uint8_t zeros[ZERO_UBO_CACHE_BYTES];
    static VkSpan s_span;
    static uint64_t s_seq;
    if (size > ZERO_UBO_CACHE_BYTES) {
        uint8_t *z = calloc(1, (size_t)size);
        VkSpan r = vk_transient_copy(z, size, 256);
        free(z);
        return r;
    }
    uint64_t seq = vk_pending_seq();
    if (s_seq != seq) {
        s_span = vk_transient_copy(zeros, ZERO_UBO_CACHE_BYTES, 256);
        s_seq = seq;
    }
    return s_span;
}

// Constant buffer k of a stage (renderer.m bind_cbufs), copied into transient memory.
static VkDescriptorBufferInfo cbuf_info(int stage, ShaderInfo *si, int k, uint32_t gx, uint32_t gy, uint32_t gz) {
    if (k == 0) {
        uint64_t size = si->cbuf_size[0] > 0x800 ? 0x800 : (si->cbuf_size[0] ? si->cbuf_size[0] : 256);
        if (size < 0x500 && stage == 5) size = 0x500;
        if (stage != 5) {
            bool any_sbuf = false;
            for (int i = 0; i < 16; i++) {
                if (st.sbuf[stage][i].addr) { any_sbuf = true; break; }
            }
            if (!any_sbuf) return ubo_span(zero_span_cached(size), size);
        }
        uint32_t drv[0x800 / 4];
        memset(drv, 0, (size_t)size);
        if (stage == 5) {
            drv[0x200 / 4] = gx; drv[0x204 / 4] = gy; drv[0x208 / 4] = gz;
            for (int i = 0; i < 16; i++) {
                uint32_t *u = &drv[(0x210 + 16 * i) / 4];
                u[0] = (uint32_t)st.ubuf[5][i].addr; u[1] = (uint32_t)(st.ubuf[5][i].addr >> 32); u[2] = (uint32_t)st.ubuf[5][i].size;
                uint32_t *e = &drv[(0x310 + 16 * i) / 4];
                e[0] = (uint32_t)st.sbuf[5][i].addr; e[1] = (uint32_t)(st.sbuf[5][i].addr >> 32); e[2] = (uint32_t)st.sbuf[5][i].size;
            }
        } else {
            uint32_t base = 0x110 + 0x100 * (stage == 1 ? 4 : 0);
            for (int i = 0; i < 16; i++) {
                uint32_t off = base + 16 * (uint32_t)i;
                if (off + 12 > size) break;
                uint32_t *e = &drv[off / 4];
                e[0] = (uint32_t)st.sbuf[stage][i].addr;
                e[1] = (uint32_t)(st.sbuf[stage][i].addr >> 32);
                e[2] = (uint32_t)st.sbuf[stage][i].size;
            }
        }
        return ubo_span(copy_dedupe(stage == 5 ? DEDUPE_DRV_C : stage == 1 ? DEDUPE_DRV_F : DEDUPE_DRV_V, drv, size, 256), size);
    }
    if (k == 1) {
        NvnShaderStage *ss = &st.program->stages[stage];
        uint64_t size = ss->const_size ? ss->const_size : 16;
        if (size < si->cbuf_size[1]) size = si->cbuf_size[1];
        return ubo_span(guest_copy(ss->code_addr + ss->const_offset, size), size);
    }
    int idx = k - 3;
    Range r = (idx >= 0 && idx < 18) ? st.ubuf[stage][idx] : (Range){0, 0};
    uint64_t size = si->cbuf_size[k] ? si->cbuf_size[k] : 256;
    if (!r.addr) return ubo_span(zero_span_cached(size), size);
    if ((si->cbuf_dynamic & (1u << k)) && r.size) size = r.size > 0x10000 ? 0x10000 : r.size;
    if (size > r.size && r.size) size = r.size;
    if (size < 16) size = 16;
    return ubo_span(guest_copy(r.addr, size), size);
}

float tex_sc[2][64][2];
uint32_t tex_wh[2][64][2];

void texture_info(int stage, ShaderInfo *si, int i, VkDescriptorImageInfo *out, float sc[2], uint32_t wh[2]) {
    ShaderTexture *st_ = &si->tex[i];
    int slot = ((int)st_->handle_offset - 0x20) / 8;
    uint64_t h = (slot >= 0 && slot < 32) ? st.tex[stage][slot] : 0;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler smp = VK_NULL_HANDLE;
    uint32_t tic = h & 0xFFFFFu, tsc = (uint32_t)(h >> 20) & 0xFFFu;
    sc[0] = sc[1] = 1;
    wh[0] = wh[1] = 0;
    if (h && st.tpool && tic < (uint32_t)st.tpool->count) {
        NvnTexEntry *e = &((NvnTexEntry *)st.tpool->entries)[tic];
        if (e->tex) {
            extern uint32_t vk_view_gen;
            extern uint64_t vk_frame_counter(void);
            static int nomemo = -1;
            if (nomemo < 0) nomemo = getenv("LARECOMP_VK_NO_TEX_MEMO") != NULL;
            typedef struct {
                NvnTexture *tex; uint64_t vh, frame; uint32_t gen;
                uint8_t type, has_view; VkImageView view;
            } TexMemoEnt;
            static TexMemoEnt memo[TEX_MEMO_BUCKETS][2];
            uint64_t vh = e->has_view ? hash_words(0, &e->view, sizeof e->view) : 0, frame = vk_frame_counter();
            uint32_t bkt = (uint32_t)((((uintptr_t)e->tex >> 4) ^ vh ^ (uint64_t)st_->type * 0x9E3779B97F4A7C15ull) & (TEX_MEMO_BUCKETS - 1));
            TexMemoEnt *hit = NULL;
            if (!nomemo) {
                for (int w = 0; w < 2; w++) {
                    TexMemoEnt *m = &memo[bkt][w];
                    if (m->tex == e->tex && m->vh == vh && m->frame == frame && m->gen == vk_view_gen &&
                        m->type == st_->type && m->has_view == e->has_view) {
                        hit = m;
                        break;
                    }
                }
            }
            if (hit) {
                view = hit->view;
            } else {
                view = vk_view(naliases ? alias_read(e->tex) : e->tex, &e->view, e->has_view, false, st_->type);
                int dst = (memo[bkt][0].frame != frame) ? 0 : (memo[bkt][1].frame != frame) ? 1 : ((int)(vh ^ frame) & 1);
                memo[bkt][dst] = (TexMemoEnt){e->tex, vh, frame, vk_view_gen, (uint8_t)st_->type, e->has_view, view};
            }
            renderer_texture_scale(e->tex, &sc[0], &sc[1]);
            wh[0] = e->tex->width; wh[1] = e->tex->height;
            if (g_nfeedback) view = feedback_lookup(e->tex, st_->type, view);
        }
    }
    if (h && st.spool && tsc < (uint32_t)st.spool->count) {
        NvnSamplerDesc *sd = &((NvnSamplerDesc *)st.spool->entries)[tsc];
        smp = sd->backend ? (VkSampler)sd->backend : vk_sampler(sd);
    }
    if (!view) view = vk_dummy_view(st_->type);
    if (!smp) smp = vk_default_sampler(true);
    *out = (VkDescriptorImageInfo){smp, view, VK_IMAGE_LAYOUT_GENERAL};
}

typedef struct { uint32_t binding, type; uint64_t a, b, c; } DescSig;
typedef struct {
    uint64_t seq, hash;
    VkDescriptorSetLayout layout;
    VkDescriptorSet ds;
    int n;
    DescSig sig[DS_CACHE_MAX_BIND];
} DsCacheEnt;

static VkSpan rscale_span(int sg, ShaderInfo *si, const float *rscale_row0) {
    static float rs[2][66][4] = {{{1, 1, 0, 0}}, {{1, 1, 0, 0}}};
    static int rs_prev_ntex[2] = {64, 64};
    static bool rs_unit[2], rs_has[2];
    static float rs_last_r0[2][4], rs_last_mw[2];
    static uint64_t rs_last_seq[2];
    static VkSpan rs_last_span[2];

    float row0[4] = {1.0f, 1.0f, 0.0f, 0.0f};
    if (rscale_row0) memcpy(row0, rscale_row0, sizeof row0);
    int ntex = si->ntex < 64 ? si->ntex : 64;
    bool all_unit = true;
    for (int t = 0; t < ntex; t++) {
        if (tex_sc[sg][t][0] != 1.0f || tex_sc[sg][t][1] != 1.0f) { all_unit = false; break; }
    }
    uint64_t seq = vk_pending_seq();
    if (all_unit && rs_has[sg] && rs_unit[sg] && rs_last_seq[sg] == seq &&
        rs_last_mw[sg] == g_mirror_w && !memcmp(rs_last_r0[sg], row0, sizeof row0)) {
        return rs_last_span[sg];
    }
    memcpy(rs[sg][0], row0, sizeof row0);
    rs[sg][65][0] = g_mirror_w;
    for (int t = 0; t < ntex; t++) {
        rs[sg][1 + t][0] = tex_sc[sg][t][0];
        rs[sg][1 + t][1] = tex_sc[sg][t][1];
    }
    if (rs_prev_ntex[sg] > ntex) {
        memset(&rs[sg][1 + ntex], 0, (size_t)(rs_prev_ntex[sg] - ntex) * sizeof rs[0][0]);
    }
    rs_prev_ntex[sg] = ntex;
    VkSpan sp = copy_dedupe(sg ? DEDUPE_RSCALE_F : DEDUPE_RSCALE_V, rs[sg], sizeof rs[0], 256);
    if (all_unit) {
        memcpy(rs_last_r0[sg], row0, sizeof row0);
        rs_last_mw[sg] = g_mirror_w;
        rs_last_seq[sg] = seq;
        rs_last_span[sg] = sp;
        rs_unit[sg] = true;
        rs_has[sg] = true;
    } else {
        rs_unit[sg] = false;
    }
    return sp;
}

float g_mirror_w;  // mirror mode's composite: its render target width (rscale[65].x; 0 otherwise)

VkDescriptorSet stage_set(Prog *p, VkDescriptorSetLayout layout, uint32_t set, int stage, ShaderInfo *si, const float *rscale_row0,
                         const float tint[8], uint32_t gx, uint32_t gy, uint32_t gz, const VkDescriptorImageInfo *pre_ii) {
    static int desc_cache_on = -1;
    if (desc_cache_on < 0) {
        const char *e = getenv("LARECOMP_VK_DESC_CACHE");
        desc_cache_on = !e || atoi(e) != 0;
    }
    static DsCacheEnt ds_cache[DS_CACHE_BUCKETS][2];
    static uint8_t ds_rr[DS_CACHE_BUCKETS];

    DescSig sig[DS_CACHE_MAX_BIND];
    VkDescriptorBufferInfo bi[MAX_BIND];
    VkDescriptorImageInfo ii[MAX_BIND];
    uint8_t is_img[MAX_BIND];
    uint32_t bind_idx[MAX_BIND];
    VkDescriptorType bind_type[MAX_BIND];
    int n = 0;
    int sg = stage == 1 ? 1 : 0;
    bool has_rscale = false;
    uint64_t h = (uint64_t)(uintptr_t)layout * 0x9E3779B97F4A7C15ull;

    for (int i = 0; i < p->nbinds; i++) {
        VkBindingInfo *b = &p->binds[i];
        if (b->set != set) continue;
        if (b->type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && b->binding == 18) {
            has_rscale = true;
            continue;
        }
        DescSig cur = {.binding = b->binding, .type = (uint32_t)b->type};
        if (b->type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && b->binding >= 32) {
            int ti = (int)b->binding - 32;
            float sc[2]; uint32_t wh[2];
            if (ti < si->ntex && pre_ii && ti < 64) {
                ii[n] = pre_ii[ti]; sc[0] = tex_sc[sg][ti][0]; sc[1] = tex_sc[sg][ti][1];
                wh[0] = tex_wh[sg][ti][0]; wh[1] = tex_wh[sg][ti][1];
            } else if (ti < si->ntex) {
                texture_info(stage, si, ti, &ii[n], sc, wh);
            } else {
                ii[n] = (VkDescriptorImageInfo){vk_default_sampler(true), vk_dummy_view(TEXTYPE_2D), VK_IMAGE_LAYOUT_GENERAL};
                sc[0] = sc[1] = 1; wh[0] = wh[1] = 0;
            }
            if (ti < 64) {
                tex_sc[sg][ti][0] = sc[0]; tex_sc[sg][ti][1] = sc[1];
                tex_wh[sg][ti][0] = wh[0]; tex_wh[sg][ti][1] = wh[1];
            }
            cur.a = (uint64_t)(uintptr_t)ii[n].sampler;
            cur.b = (uint64_t)(uintptr_t)ii[n].imageView;
            cur.c = (uint64_t)ii[n].imageLayout;
            is_img[n] = 1;
        } else if (b->type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE && b->binding >= 32) {
            int ti = (int)b->binding - 32;
            NvnTexture *t = ti < si->ntex ? shader_texture_object(stage, &si->tex[ti]) : NULL;
            VkImageView view = t ? vk_storage_view(t, si->tex[ti].type) : VK_NULL_HANDLE;
            if (!view) view = vk_dummy_storage_view(ti < si->ntex ? si->tex[ti].type : TEXTYPE_2D);
            ii[n] = (VkDescriptorImageInfo){VK_NULL_HANDLE, view, VK_IMAGE_LAYOUT_GENERAL};
            cur.a = 0; cur.b = (uint64_t)(uintptr_t)view; cur.c = (uint64_t)VK_IMAGE_LAYOUT_GENERAL;
            is_img[n] = 1;
        } else if (b->type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && b->binding < 17) {
            bi[n] = cbuf_info(stage, si, (int)b->binding, gx, gy, gz);
            cur.a = (uint64_t)(uintptr_t)bi[n].buffer; cur.b = bi[n].offset; cur.c = bi[n].range;
            is_img[n] = 0;
        } else if (b->type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
            if (gmem_is_mirror() && stage >= 0 && stage < 6)
                for (int k = 0; k < 16; k++) if (st.sbuf[stage][k].addr) gmem_interest(st.sbuf[stage][k].addr, st.sbuf[stage][k].size);
            bi[n] = b->binding == GFAST_BUFFER_INDEX ? gfast_info() : gmem_info();
            cur.a = (uint64_t)(uintptr_t)bi[n].buffer; cur.b = bi[n].offset; cur.c = bi[n].range;
            is_img[n] = 0;
        } else if (b->type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && b->binding == GMAP_BUFFER_INDEX) {
            bi[n] = gmap_info();
            cur.a = (uint64_t)(uintptr_t)bi[n].buffer; cur.b = bi[n].offset; cur.c = bi[n].range;
            is_img[n] = 0;
        } else if (b->type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && b->binding == 19) {
            bi[n] = tint ? ubo_span(copy_dedupe(DEDUPE_TINT, tint, 32, 256), 32) : ubo_span(zero_span_cached(32), 32);
            cur.a = (uint64_t)(uintptr_t)bi[n].buffer; cur.b = bi[n].offset; cur.c = bi[n].range;
            is_img[n] = 0;
        } else {
            continue;
        }
        bind_idx[n] = b->binding;
        bind_type[n] = b->type;
        if (n < DS_CACHE_MAX_BIND) {
            sig[n] = cur;
            h = hash_words(h, &cur, sizeof cur);
        }
        n++;
    }
    if (has_rscale) {
        VkSpan sp = rscale_span(sg, si, rscale_row0);
        bi[n] = ubo_span(sp, 66 * 16);
        bind_idx[n] = 18;
        bind_type[n] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        is_img[n] = 0;
        if (n < DS_CACHE_MAX_BIND) {
            DescSig cur = {.binding = 18, .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                           .a = (uint64_t)(uintptr_t)bi[n].buffer, .b = bi[n].offset, .c = bi[n].range};
            sig[n] = cur;
            h = hash_words(h, &cur, sizeof cur);
        }
        n++;
    }
    uint64_t seq = vk_pending_seq();
    uint32_t bkt = (uint32_t)(h ^ (h >> 32)) & (DS_CACHE_BUCKETS - 1);
    if (desc_cache_on && n <= DS_CACHE_MAX_BIND) {
        for (int w = 0; w < 2; w++) {
            DsCacheEnt *e = &ds_cache[bkt][w];
            if (e->seq == seq && e->hash == h && e->layout == layout && e->n == n &&
                (!n || !memcmp(e->sig, sig, (size_t)n * sizeof sig[0]))) {
                return e->ds;
            }
        }
    }
    VkDescriptorSet ds = vk_alloc_set(layout);
    VkWriteDescriptorSet w[MAX_BIND];
    for (int k = 0; k < n; k++) {
        w[k] = (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds, .dstBinding = bind_idx[k],
            .descriptorCount = 1, .descriptorType = bind_type[k],
            .pImageInfo = is_img[k] ? &ii[k] : NULL, .pBufferInfo = is_img[k] ? NULL : &bi[k]};
    }
    if (n) vkUpdateDescriptorSets(g_vk.dev, (uint32_t)n, w, 0, NULL);
    if (desc_cache_on && n <= DS_CACHE_MAX_BIND) {
        int way = ds_cache[bkt][0].seq != seq ? 0 : ds_cache[bkt][1].seq != seq ? 1 : (ds_rr[bkt]++ & 1);
        DsCacheEnt *e = &ds_cache[bkt][way];
        e->seq = seq; e->hash = h; e->layout = layout; e->ds = ds; e->n = n;
        if (n) memcpy(e->sig, sig, (size_t)n * sizeof sig[0]);
    }
    return ds;
} // [extra]
