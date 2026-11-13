/* Astral Chain shim for LASRecomp's settings: fixed defaults, no settings UI.
 * Enum values follow LASRecomp runtime/settings.h. */
#pragma once
#include <stdio.h> // [extra]
#include <stdlib.h> // [extra]
#include <string.h> // [extra]
#include "additions/toggles/ac_toggles.h" // [extra]
enum { UPSCALE_BILINEAR = 0, UPSCALE_METALFX = 1, UPSCALE_FSR1 = 2, UPSCALE_FSR3 = 3 }; // [extra]
enum { AA_OFF = 0, AA_SMAA = 1 };
// [extra] Language definitions and Settings struct // [extra]
enum { LANG_AUTO = 0, N_LANGUAGES = 13 }; // [extra]
static const char *const g_language_codes[N_LANGUAGES] = {
    "auto", "ja", "en-US", "es", "fr", "de", "it", "nl", "ru", "ko", "zh-CN", "zh-TW", "pt-BR" // [extra]
};
static const char *const g_language_names[N_LANGUAGES] = {
    "Auto", "Japonês", "Inglês", "Espanhol", "Francês", "Alemão", "Italiano", "Holandês", "Russo", "Coreano", "Chinês Simplificado", "Chinês Tradicional", "Português (Brasil)" // [extra]
};
typedef struct { int antialias, fullscreen, upscaler, language, voice_language, framegen, fsr_jitter; float fsr_sharpness, render_scale; } Settings; // [extra]
extern Settings g_settings;
// [extra] Language helpers for overlay menu and runtime // [extra]
static inline const char *settings_language(void) {
    const char *e = getenv("AC_LANGUAGE");
    if (!e || !*e) e = getenv("LARECOMP_LANG");
    if (e && *e && strcmp(e, "auto") != 0) {
        for (int i = 1; i < N_LANGUAGES; i++) {
            if (!strcmp(e, g_language_codes[i])) return g_language_codes[i];
        }
    }
    int idx = g_settings.language;
    if (idx > LANG_AUTO && idx < N_LANGUAGES) return g_language_codes[idx];
    const char *loc = getenv("LC_ALL");
    if (!loc || !*loc) loc = getenv("LC_MESSAGES");
    if (!loc || !*loc) loc = getenv("LANG");
    if (loc && *loc) {
        if (!strncmp(loc, "ja", 2)) return "ja";
        if (!strncmp(loc, "pt", 2)) return "pt-BR"; // [extra]
        if (!strncmp(loc, "es", 2)) return "es";
        if (!strncmp(loc, "fr", 2)) return "fr";
        if (!strncmp(loc, "de", 2)) return "de";
        if (!strncmp(loc, "it", 2)) return "it";
        if (!strncmp(loc, "ru", 2)) return "ru";
        if (!strncmp(loc, "ko", 2)) return "ko";
        if (!strncmp(loc, "nl", 2)) return "nl";
        if (!strncmp(loc, "zh", 2)) return strstr(loc, "TW") || strstr(loc, "HK") ? "zh-TW" : "zh-CN";
    }
    return "en-US";
}

static inline const char *settings_language_name(void) {
    const char *code = settings_language();
    for (int i = 0; i < N_LANGUAGES; i++) {
        if (!strcmp(code, g_language_codes[i])) return g_language_names[i];
    }
    return "Inglês";
}

static inline int extra_settings_bool(const char *val) { // [extra]
    return !strcmp(val, "true") || !strcmp(val, "on") || !strcmp(val, "yes") || atoi(val) != 0; // [extra]
} // [extra]
static inline void settings_save(void) { // [extra]
    ac_config_sync_save_hook(); // [extra]
} // [extra]
static inline void settings_load(void) { // [extra]
    g_settings.antialias = AA_SMAA; // [extra]
    g_settings.upscaler = UPSCALE_FSR3; // [extra]
    g_settings.fsr_jitter = 1;
    g_settings.fsr_sharpness = 0.80f;
    g_settings.framegen = 0;
    g_settings.render_scale = 1.0f; // [extra]
    const char *u = getenv("AC_UPSCALER");
    if (u) {
        if (!strcmp(u, "fsr3") || !strcmp(u, "3")) g_settings.upscaler = UPSCALE_FSR3; // [extra]
        else if (!strcmp(u, "fsr1") || !strcmp(u, "fsr") || !strcmp(u, "2")) g_settings.upscaler = UPSCALE_FSR1; // [extra]
        else if (!strcmp(u, "bilinear") || !strcmp(u, "none") || !strcmp(u, "0")) g_settings.upscaler = UPSCALE_BILINEAR; // [extra]
    }
    const char *aa = getenv("AC_ANTIALIAS");
    if (aa) {
        if (!strcmp(aa, "smaa") || !strcmp(aa, "1")) g_settings.antialias = AA_SMAA;
        else if (!strcmp(aa, "off") || !strcmp(aa, "none") || !strcmp(aa, "0")) g_settings.antialias = AA_OFF; // [extra]
    }
    FILE *f = fopen("userdata/launcher.ini", "r"); // [extra]
    if (!f) f = fopen("larecomp.ini", "r"); // [extra]
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            char key[32], val[64];
            if (sscanf(line, " %31[^= \t\r\n] = %63[^\r\n]", key, val) != 2) continue; // [extra]
            size_t vlen = strlen(val);
            while (vlen > 0 && (val[vlen - 1] == ' ' || val[vlen - 1] == '\t')) val[--vlen] = '\0';
            if (!strcmp(key, "language")) { // [extra]
                for (int i = 0; i < N_LANGUAGES; i++) {
                    if (!strcmp(val, g_language_codes[i])) { g_settings.language = i; break; }
                }
            } else if (!strcmp(key, "voice_language")) { // [extra]
                g_settings.voice_language = !strcmp(val, "ja") ? 1 : !strcmp(val, "en") ? 2 : 0;
            } else if (!strcmp(key, "upscaler") && !u) { // [extra]
                g_settings.upscaler = !strcmp(val, "fsr3") ? UPSCALE_FSR3 : !strcmp(val, "fsr1") ? UPSCALE_FSR1 : UPSCALE_BILINEAR;
            } else if (!strcmp(key, "antialias") && !aa) { // [extra]
                g_settings.antialias = (!strcmp(val, "off") || !strcmp(val, "none") || !strcmp(val, "0")) ? AA_OFF : AA_SMAA;
            } else if (!strcmp(key, "framegen") || !strcmp(key, "frame_gen")) { // [extra]
                g_settings.framegen = extra_settings_bool(val);
            } else if (!strcmp(key, "fsr_jitter")) {
                g_settings.fsr_jitter = extra_settings_bool(val);
            } else if (!strcmp(key, "fsr_sharpness")) { // [extra]
                g_settings.fsr_sharpness = (float)atof(val);
            } else if (!strcmp(key, "render_scale")) { // [extra]
                g_settings.render_scale = (float)atof(val);
            } // [extra]
        }
        fclose(f);
    } // [extra]
    const char *lang = getenv("AC_LANGUAGE");
    if (!lang || !*lang) lang = getenv("LARECOMP_LANG");
    if (lang && *lang) {
        for (int i = 0; i < N_LANGUAGES; i++) {
            if (!strcmp(lang, g_language_codes[i])) { g_settings.language = i; break; }
        }
    } // [extra]
    const char *vlang = getenv("AC_VOICE_LANGUAGE");
    if (!vlang || !*vlang) vlang = getenv("LARECOMP_VOICE_LANG");
    if (vlang && *vlang) {
        if (!strcmp(vlang, "ja") || !strcmp(vlang, "jpn") || !strcmp(vlang, "1")) g_settings.voice_language = 1;
        else if (!strcmp(vlang, "en") || !strcmp(vlang, "usa") || !strcmp(vlang, "2")) g_settings.voice_language = 2;
        else g_settings.voice_language = 0;
    }
    const char *fg = getenv("AC_FRAMEGEN"); // [extra]
    if (fg && *fg) g_settings.framegen = extra_settings_bool(fg); // [extra]
    const char *jit = getenv("AC_FSR_JITTER");
    if (jit && *jit) g_settings.fsr_jitter = extra_settings_bool(jit); // [extra]
    const char *shp = getenv("AC_FSR_SHARPNESS");
    if (shp && *shp) g_settings.fsr_sharpness = (float)atof(shp);
    const char *rsc = getenv("AC_RENDER_SCALE");
    if (rsc && *rsc) g_settings.render_scale = (float)atof(rsc);
    if (g_settings.fsr_sharpness < 0.0f) g_settings.fsr_sharpness = 0.0f;
    if (g_settings.fsr_sharpness > 1.0f) g_settings.fsr_sharpness = 1.0f;
    if (g_settings.render_scale < 0.5f || g_settings.render_scale > 2.0f) g_settings.render_scale = 1.0f; // [extra]
    ac_config_sync_init_hook(); // [extra]
} // [extra]
