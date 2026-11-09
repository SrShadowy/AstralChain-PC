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

/* FSR 3.1 Upscaler, Camera Jitter, RCAS Sharpness & Frame Generation overrides. */
extern int   g_extra_framegen;       /* -1: AC_FRAMEGEN / settings; 0/1: forced */
extern int   g_extra_fsr_jitter;     /* -1: AC_FSR_JITTER / settings; 0/1: forced */
extern float g_extra_fsr_sharpness;  /* <0: AC_FSR_SHARPNESS / settings; 0..1: forced */
extern float g_extra_render_scale;   /* <=0: AC_RENDER_SCALE / settings; 0.5..2.0: forced */

typedef struct {
    float jitter_px[2];       /* Current frame Halton(2,3) jitter in render pixels [-0.5, +0.5] */
    float prev_jitter_px[2];  /* Previous frame Halton(2,3) jitter in render pixels */
    float upscale_ratio;      /* Output width / render width */
    unsigned int phase_index; /* Halton sequence counter */
    int jitter_active;        /* 1 when FSR 3.1 + Camera Jitter is active this frame */
    int fg_presenting_interp; /* 1 whenvk_present_draw should draw interpolated frame U_{t-0.5} */
    int fg_ready;             /* 1 when U_{t-0.5} was generated for the current frame */
} ExtraFsr3State;

extern ExtraFsr3State g_extra_fsr3;

/* Applies the async kill switch: flag new submits out of the worker queue,
 * then drain so no queued job can be overtaken by a later sync submit. */
void extra_async_submit_kill(void);

#ifdef __cplusplus
}
#endif
