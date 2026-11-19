// [extra]
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "../../exec_internal.h"

#define NVN_PRIM_POINTS         0u
#define NVN_PRIM_LINES          1u
#define NVN_PRIM_LINE_STRIP     3u
#define NVN_PRIM_TRIANGLES      4u
#define NVN_PRIM_TRIANGLE_STRIP 5u
#define NVN_PRIM_TRIANGLE_FAN   6u
#define NVN_PRIM_QUADS          7u
#define NVN_PRIM_POLYGON        9u

#define AC_MAX_TEX_SLOTS        64

typedef struct {
    uint32_t count, icount, inst, first, base_inst, max_index;
    int32_t base_vertex;
    bool use_index, small;
    const void *scan_src;
    uint32_t scan_isz, scan_restart;
} DrawGeomInfo;

extern uint64_t g_cur_fs_hash;

void ct_draw(Prog *pr, const char *what);
void debug_draw(Prog *pr, CmdRec *r, bool indexed);
bool fs_skip_or_watch(Prog *pr, CmdRec *r);

void bind_draw_indices(VkCommandBuffer cb, Prog *pr, CmdRec *r, bool indexed, bool native, bool dt, uint32_t mode, DrawGeomInfo *g);
void resolve_deferred_index_scan(DrawGeomInfo *g);
void bind_vertex_streams(VkCommandBuffer cb, const PipeKey *pk, DrawGeomInfo *g);
// [extra]
