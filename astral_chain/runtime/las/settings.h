/* Astral Chain shim for LASRecomp's settings: fixed defaults, no settings UI.
 * Enum values follow LASRecomp runtime/settings.h. */
#pragma once
#include <stdio.h> // [extra]
#include <stdlib.h> // [extra]
#include <string.h> // [extra]
enum { UPSCALE_BILINEAR = 0, UPSCALE_METALFX = 1, UPSCALE_FSR1 = 2 };
enum { AA_OFF = 0, AA_SMAA = 1 };
// [extra] Language definitions and Settings struct // [extra]
enum { LANG_AUTO = 0, N_LANGUAGES = 13 }; // [extra]
static const char *const g_language_codes[N_LANGUAGES] = {
    "auto", "ja", "en-US", "es", "fr", "de", "it", "nl", "ru", "ko", "zh-CN", "zh-TW", "pt-BR" // [extra]
};
static const char *const g_language_names[N_LANGUAGES] = {
    "Auto", "Japonês", "Inglês", "Espanhol", "Francês", "Alemão", "Italiano", "Holandês", "Russo", "Coreano", "Chinês Simplificado", "Chinês Tradicional", "Português (Brasil)" // [extra]
};
typedef struct { int antialias, fullscreen, upscaler, language, voice_language; } Settings; // [extra]
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

static inline void settings_save(void) {
    FILE *f = fopen("larecomp.ini", "w");
    if (!f) return;
    fprintf(f, "# Astral Chain settings\n");
    int idx = g_settings.language >= 0 && g_settings.language < N_LANGUAGES ? g_settings.language : 0;
    fprintf(f, "language = %s\n", g_language_codes[idx]);
    // [extra]
    static const char *const vlangs[] = {"auto", "ja", "en"};
    int vidx = g_settings.voice_language >= 0 && g_settings.voice_language <= 2 ? g_settings.voice_language : 0;
    fprintf(f, "voice_language = %s\n", vlangs[vidx]);
    // [extra]
    fclose(f);
} // [extra]
static inline void settings_load(void) { // [extra]
    const char *u = getenv("AC_UPSCALER");
    if (u) {
        if (!strcmp(u, "fsr1") || !strcmp(u, "fsr") || !strcmp(u, "2")) g_settings.upscaler = UPSCALE_FSR1;
        else if (!strcmp(u, "bilinear") || !strcmp(u, "0")) g_settings.upscaler = UPSCALE_BILINEAR;
    }
    const char *aa = getenv("AC_ANTIALIAS");
    if (aa) {
        if (!strcmp(aa, "smaa") || !strcmp(aa, "1")) g_settings.antialias = AA_SMAA;
        else if (!strcmp(aa, "off") || !strcmp(aa, "0")) g_settings.antialias = AA_OFF;
    }
    // [extra] Language persistence in larecomp.ini // [extra]
    FILE *f = fopen("larecomp.ini", "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            char key[32], val[64];
            if (sscanf(line, " %31[^= ] = %63[^\n]", key, val) == 2 && !strcmp(key, "language")) {
                for (int i = 0; i < N_LANGUAGES; i++) {
                    if (!strcmp(val, g_language_codes[i])) { g_settings.language = i; break; }
                }
            }
            // [extra]
            else if (sscanf(line, " %31[^= ] = %63[^\n]", key, val) == 2 && !strcmp(key, "voice_language")) {
                g_settings.voice_language = !strcmp(val, "ja") ? 1 : !strcmp(val, "en") ? 2 : 0;
            } // [extra]
        }
        fclose(f);
    }
    const char *lang = getenv("AC_LANGUAGE");
    if (!lang || !*lang) lang = getenv("LARECOMP_LANG");
    if (lang && *lang) {
        for (int i = 0; i < N_LANGUAGES; i++) {
            if (!strcmp(lang, g_language_codes[i])) { g_settings.language = i; break; }
        }
    } // [extra]
    // [extra]
    const char *vlang = getenv("AC_VOICE_LANGUAGE");
    if (!vlang || !*vlang) vlang = getenv("LARECOMP_VOICE_LANG");
    if (vlang && *vlang) {
        if (!strcmp(vlang, "ja") || !strcmp(vlang, "jpn") || !strcmp(vlang, "1")) g_settings.voice_language = 1;
        else if (!strcmp(vlang, "en") || !strcmp(vlang, "usa") || !strcmp(vlang, "2")) g_settings.voice_language = 2;
        else g_settings.voice_language = 0;
    }
    // [extra]
} // [extra]
