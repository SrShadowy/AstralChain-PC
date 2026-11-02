/* [extra] Runtime graphics/debug toggles shared by the NVN layer and the ImGui
 * overlay (extra/ui/overlay/). Every value falls through to its environment
 * variable while untouched, so the menu is a pure override on top of the
 * launcher/env configuration; a fresh game launch starts from env again. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern int g_extra_vtx_guard;        /* -1: LARECOMP_GUARD_VTX env; 0/1: forced */
extern int g_extra_vtx_debug;        /* -1: LARECOMP_DEBUG_VTX env; 0/1: forced */
extern int g_extra_fps_override;     /* 0: AC_FPS env; >0: forced target fps */
extern int g_extra_async_kill;       /* 1: force async submission off for new submits */
extern const char *g_extra_present_mode; /* NULL: LARECOMP_VK_PRESENT env; else fifo/mailbox/immediate/fifo_relaxed */

/* Graphics overrides (texture quality / LoD / shadow): same -1 == "fall through to
 * env" contract as g_extra_vtx_guard. Positive LOD bias = lower mip = lighter textures. */
extern float g_extra_lod_bias;       /* -1: LARECOMP_LOD_BIAS env; >=0: forced global LOD bias */
extern float g_extra_lod_min;        /* -1: LARECOMP_LOD_MIN env; >=0: forced min-LOD floor */
extern int   g_extra_no_shadow;      /* -1: LARECOMP_NO_SHADOW env; 0/1: forced shadow off */

/* Applies the async kill switch: flag new submits out of the worker queue,
 * then drain so no queued job can be overtaken by a later sync submit. */
void extra_async_submit_kill(void);

#ifdef __cplusplus
}
#endif
