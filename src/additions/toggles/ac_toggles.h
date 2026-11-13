/* [extra] Runtime graphics/debug/input toggles shared by the NVN layer, SDL/HID hooks,
 * config sync, and the ImGui overlay. Every value falls through to its environment variable
 * while untouched, and live INI/UI updates override smoothly at runtime. */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EXTRA_TOGGLE_UNSET         (-1)
#define EXTRA_AUDIO_VOL_MAX        100
#define EXTRA_KEY_SLOTS            2
#define EXTRA_KEY_ACTIONS          24
#define EXTRA_MOUSE_CELL_DEFAULT   40.0f
#define EXTRA_MOUSE_CURVE_DEFAULT  1.35

enum ExtraPromptMode {
    EXTRA_PROMPT_MODE_SWITCH = 0,
    EXTRA_PROMPT_MODE_KBM = 1,
    EXTRA_PROMPT_MODE_XBOX = 2,
    EXTRA_PROMPT_MODE_AUTO = 3
};

extern int g_extra_vtx_guard;        /* -1: LARECOMP_GUARD_VTX env; 0/1: forced */
extern int g_extra_vtx_debug;        /* -1: LARECOMP_DEBUG_VTX env; 0/1: forced */
extern int g_extra_fps_override;     /* -1: AC_FPS env; >=0: forced target fps (0=unlimited) */
extern int g_extra_async_kill;       /* 1: force async submission off for new submits */
extern const char *g_extra_present_mode; /* NULL: LARECOMP_VK_PRESENT env; else fifo/mailbox/immediate/fifo_relaxed */

/* Graphics overrides (texture quality / LoD / shadow): same -1 == "fall through to env" contract. */
extern float g_extra_lod_bias;       /* -1: LARECOMP_LOD_BIAS env; >=0: forced global LOD bias */
extern float g_extra_lod_min;        /* -1: LARECOMP_LOD_MIN env; >=0: forced min-LOD floor */
extern int   g_extra_no_shadow;      /* -1: LARECOMP_NO_SHADOW env; 0/1: forced shadow off */

/* FSR 3.1 Upscaler, Camera Jitter, RCAS Sharpness & Frame Generation overrides. */
extern int   g_extra_framegen;       /* -1: AC_FRAMEGEN / settings; 0/1: forced */
extern int   g_extra_fsr_jitter;     /* -1: AC_FSR_JITTER / settings; 0/1: forced */
extern float g_extra_fsr_sharpness;  /* <0: AC_FSR_SHARPNESS / settings; 0..1: forced */
extern float g_extra_render_scale;   /* <=0: AC_RENDER_SCALE / settings; 0.5..2.0: forced */

/* Window, prompts, focus, cutscene, audio, mouse & keybinding live overrides (shared across TUs). */
extern int      g_extra_fullscreen_pending; /* -1: none; 0/1: apply fullscreen on SDL thread */
extern int      g_extra_win_w;              /* <=0: unchanged; >0: target window width */
extern int      g_extra_win_h;              /* <=0: unchanged; >0: target window height */
extern int      g_extra_win_resize_pending; /* 1: apply window size on SDL thread */
extern int      g_extra_config_dirty;       /* 1: SDL thread modified state; persist to INI */
extern int      g_extra_present_pending;    /* 1: swapchain present mode recreate pending */
extern int      g_sdl_fullscreen;           /* 0/1: live window fullscreen state */
extern int      g_sdl_input_focus;          /* 0/1: live window input focus state */
extern uint32_t g_sdl_prompt_generation;    /* bumped when button prompt atlas needs rebuild */
extern int      g_extra_prompts_override;   /* -1: env; ExtraPromptMode value when overridden */
extern int      g_extra_gamepad_confirm_a;  /* -1: env; 0/1: confirm with A button on gamepad */
extern int      g_extra_ignore_focus;       /* -1: env; 0/1: forced ignore_focus */
extern int      g_extra_cutscene_30fps;     /* -1: env; 0/1: forced 30 FPS cutscenes */
extern int      g_extra_audio_volume;       /* -1: env/100; 0..100: master volume pct */
extern int      g_extra_audio_mute_unfocus; /* -1: env/0; 0/1: mute when window unfocused */
extern uint32_t g_extra_audio_gen;          /* bumped when audio gain settings change */
extern int      g_extra_mouse_override;     /* 0: env/default; 1: synced runtime values */
extern uint32_t g_extra_mouse_gen;          /* bumped when mouse settings change live */
extern int      g_extra_mouse_look;         /* 0/1: mouse controls camera */
extern double   g_extra_mouse_sens;         /* camera sensitivity multiplier (1.0 == 100%) */
extern double   g_extra_mouse_yscale;       /* vertical sensitivity scale */
extern double   g_extra_mouse_curve_exp;    /* response exponent (1.0/1.35/1.75) */
extern int      g_extra_mouse_invert;       /* 0/1: invert Y look */
extern int      g_extra_mouse_menu_nav;     /* 0/1: menu pointer navigation */
extern int      g_extra_mouse_menu_motion;  /* 0/1: pointer motion drives menu steps */
extern float    g_extra_mouse_menu_cell;    /* menu pointer step threshold in px */
extern uint8_t  g_extra_key_map[EXTRA_KEY_SLOTS][EXTRA_KEY_ACTIONS];
extern int      g_extra_key_override;       /* 0: env/default; 1: synced key map */
extern uint32_t g_extra_keys_gen;           /* bumped when [Controls] bindings update live */

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

void extra_async_submit_kill(void);

/* Cross-TU mouse sync readers (overridden weakly in standalone pointer tests). */
uint32_t extra_sync_read_mouse(int *look, double *sens, double *yscale, double *curve_exp, int *invert);
uint32_t extra_sync_read_menu_mouse(int *nav, int *motion, float *cell);

/* Weak bridge hooks overridden by ac_config_sync.cpp when the vulkan overlay is linked. */
void ac_config_sync_init_hook(void);
void ac_config_sync_save_hook(void);

#ifdef __cplusplus
}
#endif
