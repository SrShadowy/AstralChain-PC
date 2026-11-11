/* [extra] RomFS Virtual Overlay: non-destructive LayeredFS mod loader.
 * Overrides specific RomFS files (e.g. Text/Text.pkz, ui/ui.pkz) by patching
 * the in-memory file_meta table and routing reads to host files in mods/. */
#pragma once

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "hooks/platform/sync_trampoline.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EXTRA_MAX_OVERLAYS 64
#define EXTRA_OVERLAY_PATH_CAP 256
#define EXTRA_ROMFS_MAX_DIR_DEPTH 16
#define EXTRA_ROMFS_MAX_NAME_LEN 255
#define EXTRA_ROMFS_HEADER_WORDS 10
#define EXTRA_ROMFS_HEADER_BYTES 80
#define EXTRA_ROMFS_MAX_META_BYTES (64 * 1024 * 1024)
#define EXTRA_ROMFS_ALIGN_MASK 0xFFFull
#define EXTRA_ROMFS_DIR_NAMELEN_OFF 0x14
#define EXTRA_ROMFS_DIR_NAME_OFF 0x18
#define EXTRA_ROMFS_FILE_NAMELEN_OFF 0x1C
#define EXTRA_ROMFS_FILE_NAME_OFF 0x20
#define EXTRA_ROMFS_FILE_DATA_OFF 8
#define EXTRA_ROMFS_FILE_SIZE_OFF 16
#define EXTRA_ROMFS_FULL_PATH_CAP 1024
#define EXTRA_INI_LINE_CAP 128
#define EXTRA_INI_TOKEN_CAP 32

typedef struct {
    uint64_t virtual_start;
    uint64_t virtual_size;
    int fd;
    char path[EXTRA_OVERLAY_PATH_CAP];
} ExtraRomfsOverlay;

static ExtraRomfsOverlay s_extra_overlays[EXTRA_MAX_OVERLAYS];
static int s_extra_num_overlays = 0;
static uint8_t *s_extra_fmeta_buf = NULL;
static uint64_t s_extra_fmeta_off = 0;
static uint64_t s_extra_fmeta_size = 0;
static uint64_t s_extra_orig_size = 0;

static inline const char *extra_read_ini_language(const char *ini_path, char *out_buf, size_t out_cap) {
    FILE *f = fopen(ini_path, "r");
    if (!f) {
        return NULL;
    }
    char line[EXTRA_INI_LINE_CAP];
    const char *found = NULL;
    while (fgets(line, sizeof(line), f)) {
        char k[EXTRA_INI_TOKEN_CAP];
        char v[EXTRA_INI_TOKEN_CAP];
        if (sscanf(line, " %31[^= ] = %31[^\n]", k, v) == 2 && !strcmp(k, "language")) {
            strncpy(out_buf, v, out_cap - 1);
            out_buf[out_cap - 1] = '\0';
            found = out_buf;
            break;
        }
    }
    fclose(f);
    return found;
}

static inline bool extra_is_pt_language(void) {
    static char ini_lang[EXTRA_INI_TOKEN_CAP];
    const char *lang = getenv("AC_LANGUAGE");
    if (!lang || !*lang) {
        lang = getenv("LARECOMP_LANG");
    }
    if (!lang || !*lang) {
        lang = extra_read_ini_language("larecomp.ini", ini_lang, sizeof(ini_lang));
    }
    if (!lang || !*lang) {
        lang = extra_read_ini_language("userdata/launcher.ini", ini_lang, sizeof(ini_lang));
    }
    if (!lang || !*lang || !strcmp(lang, "auto")) {
        const char *loc = getenv("LC_ALL");
        if (!loc || !*loc) {
            loc = getenv("LC_MESSAGES");
        }
        if (!loc || !*loc) {
            loc = getenv("LANG");
        }
        if (loc && !strncmp(loc, "pt", 2)) {
            return true;
        }
    }
    if (lang && (!strcmp(lang, "pt-BR") || !strcmp(lang, "pt") || !strncmp(lang, "pt_", 3))) {
        return true;
    }
    if (!lang || !*lang || !strcmp(lang, "auto")) {
        struct stat st;
        if (stat("mods/traducao_ptbr/Text/Text.pkz", &st) == 0 ||
            stat("mods/traducao_ptbr/Text.pkz", &st) == 0) {
            return true;
        }
    }
    return false;
}

static inline void extra_romfs_dir_path(const uint8_t *dmeta, uint64_t dsz, uint32_t dir, char *out, size_t cap) {
    char parts[EXTRA_ROMFS_MAX_DIR_DEPTH][EXTRA_OVERLAY_PATH_CAP];
    int n = 0;
    while (dir != 0 && dir < dsz && n < EXTRA_ROMFS_MAX_DIR_DEPTH) {
        uint32_t parent = 0;
        uint32_t namelen = 0;
        memcpy(&parent, dmeta + dir, sizeof(uint32_t));
        memcpy(&namelen, dmeta + dir + EXTRA_ROMFS_DIR_NAMELEN_OFF, sizeof(uint32_t));
        if (namelen > EXTRA_ROMFS_MAX_NAME_LEN) {
            namelen = EXTRA_ROMFS_MAX_NAME_LEN;
        }
        memcpy(parts[n], dmeta + dir + EXTRA_ROMFS_DIR_NAME_OFF, namelen);
        parts[n][namelen] = '\0';
        n++;
        dir = parent;
    }
    out[0] = '\0';
    for (int i = n - 1; i >= 0; i--) {
        size_t cur = strlen(out);
        snprintf(out + cur, cap - cur, "%s/", parts[i]);
    }
}

static inline bool extra_romfs_find_mod_file(
    bool is_pt,
    const char *full_path,
    const char *name,
    char *host_candidate,
    size_t candidate_cap,
    struct stat *st
) {
    if (is_pt) {
        static const char *const pt_roots[] = {
            "mods/traducao_ptbr", "mods/ptbr", "mods/pt-BR", "mods/portugues"
        };
        for (size_t r = 0; r < sizeof(pt_roots) / sizeof(pt_roots[0]); r++) {
            snprintf(host_candidate, candidate_cap, "%s/%s", pt_roots[r], full_path);
            if (stat(host_candidate, st) == 0 && S_ISREG(st->st_mode)) {
                return true;
            }
            snprintf(host_candidate, candidate_cap, "%s/%s", pt_roots[r], name);
            if (stat(host_candidate, st) == 0 && S_ISREG(st->st_mode)) {
                return true;
            }
            snprintf(host_candidate, candidate_cap, "%s/romfs/%s", pt_roots[r], full_path);
            if (stat(host_candidate, st) == 0 && S_ISREG(st->st_mode)) {
                return true;
            }
        }
    }
    snprintf(host_candidate, candidate_cap, "mods/%s", full_path);
    if (stat(host_candidate, st) == 0 && S_ISREG(st->st_mode)) {
        return true;
    }
    snprintf(host_candidate, candidate_cap, "mods/%s", name);
    if (stat(host_candidate, st) == 0 && S_ISREG(st->st_mode)) {
        return true;
    }
    snprintf(host_candidate, candidate_cap, "mods/romfs/%s", full_path);
    if (stat(host_candidate, st) == 0 && S_ISREG(st->st_mode)) {
        return true;
    }
    return false;
}

static inline void extra_romfs_overlay_init(void *romfs_handle, uint64_t *p_romfs_size) {
    if (!romfs_handle || !p_romfs_size) {
        return;
    }
    ExtraHandle *eh = (ExtraHandle *)romfs_handle;
    int fd = eh->file.fd;
    if (fd < 0) {
        return;
    }

    s_extra_orig_size = *p_romfs_size;
    uint64_t h[EXTRA_ROMFS_HEADER_WORDS];
    if (pread(fd, h, sizeof(h), 0) != (ssize_t)sizeof(h)) {
        return;
    }
    if (h[0] != EXTRA_ROMFS_HEADER_BYTES) {
        return;
    }

    uint64_t doff = h[3];
    uint64_t dsz = h[4];
    uint64_t foff = h[7];
    uint64_t fsz = h[8];
    uint64_t data_off = h[9];
    if (fsz == 0 || fsz > EXTRA_ROMFS_MAX_META_BYTES) {
        return;
    }

    uint8_t *dmeta = (uint8_t *)malloc(dsz);
    uint8_t *fmeta = (uint8_t *)malloc(fsz);
    if (!dmeta || !fmeta) {
        free(dmeta);
        free(fmeta);
        return;
    }

    if (pread(fd, dmeta, dsz, (off_t)doff) != (ssize_t)dsz ||
        pread(fd, fmeta, fsz, (off_t)foff) != (ssize_t)fsz) {
        free(dmeta);
        free(fmeta);
        return;
    }

    bool is_pt = extra_is_pt_language();
    uint64_t virt_end = (s_extra_orig_size + EXTRA_ROMFS_ALIGN_MASK) & ~EXTRA_ROMFS_ALIGN_MASK;

    for (uint64_t o = 0; o + EXTRA_ROMFS_FILE_NAME_OFF <= fsz;) {
        uint32_t parent = 0;
        uint32_t namelen = 0;
        memcpy(&parent, fmeta + o, sizeof(uint32_t));
        memcpy(&namelen, fmeta + o + EXTRA_ROMFS_FILE_NAMELEN_OFF, sizeof(uint32_t));
        if (namelen > EXTRA_ROMFS_MAX_NAME_LEN) {
            namelen = EXTRA_ROMFS_MAX_NAME_LEN;
        }
        char name[EXTRA_OVERLAY_PATH_CAP];
        memcpy(name, fmeta + o + EXTRA_ROMFS_FILE_NAME_OFF, namelen);
        name[namelen] = '\0';

        char dir[EXTRA_ROMFS_FULL_PATH_CAP];
        extra_romfs_dir_path(dmeta, dsz, parent, dir, sizeof(dir));
        char full_path[EXTRA_ROMFS_FULL_PATH_CAP];
        snprintf(full_path, sizeof(full_path), "%s%s", dir, name);

        char host_candidate[EXTRA_ROMFS_FULL_PATH_CAP] = {0};
        struct stat st;
        bool found = extra_romfs_find_mod_file(is_pt, full_path, name, host_candidate, sizeof(host_candidate), &st);

        if (found && s_extra_num_overlays < EXTRA_MAX_OVERLAYS) {
            int mod_fd = open(host_candidate, O_RDONLY);
            if (mod_fd >= 0) {
                uint64_t mod_size = (uint64_t)st.st_size;
                uint64_t v_start = virt_end;
                uint64_t rel_off = v_start - data_off;

                memcpy(fmeta + o + EXTRA_ROMFS_FILE_DATA_OFF, &rel_off, sizeof(uint64_t));
                memcpy(fmeta + o + EXTRA_ROMFS_FILE_SIZE_OFF, &mod_size, sizeof(uint64_t));

                s_extra_overlays[s_extra_num_overlays].virtual_start = v_start;
                s_extra_overlays[s_extra_num_overlays].virtual_size = mod_size;
                s_extra_overlays[s_extra_num_overlays].fd = mod_fd;
                strncpy(s_extra_overlays[s_extra_num_overlays].path, host_candidate, EXTRA_ROMFS_MAX_NAME_LEN);
                fprintf(stderr, "[romfs_overlay] Override '%s' -> '%s' (size: %llu bytes, virt_off: 0x%llx)\n",
                        full_path, host_candidate, (unsigned long long)mod_size, (unsigned long long)v_start);
                fflush(stderr);
                virt_end = (v_start + mod_size + EXTRA_ROMFS_ALIGN_MASK) & ~EXTRA_ROMFS_ALIGN_MASK;
                s_extra_num_overlays++;
            }
        }

        o += EXTRA_ROMFS_FILE_NAME_OFF + ((namelen + 3u) & ~3u);
    }

    free(dmeta);

    if (s_extra_num_overlays > 0) {
        s_extra_fmeta_buf = fmeta;
        s_extra_fmeta_off = foff;
        s_extra_fmeta_size = fsz;
        *p_romfs_size = virt_end;
        fprintf(stderr, "[romfs_overlay] Initialized %d overlay(s), virtual RomFS size: %llu (orig: %llu)\n",
                s_extra_num_overlays, (unsigned long long)*p_romfs_size, (unsigned long long)s_extra_orig_size);
        fflush(stderr);
    } else {
        free(fmeta);
    }
}

static inline bool extra_romfs_overlay_read(void *romfs_handle, uint64_t offset, uint64_t length, void *dest) {
    if (s_extra_num_overlays == 0) {
        return false;
    }
    ExtraHandle *eh = (ExtraHandle *)romfs_handle;
    int base_fd = eh->file.fd;
    if (base_fd < 0) {
        return false;
    }

    if (offset < s_extra_orig_size) {
        uint64_t read_len = (offset + length <= s_extra_orig_size) ? length : (s_extra_orig_size - offset);
        pread(base_fd, dest, (size_t)read_len, (off_t)offset);
        if (read_len < length) {
            memset((char *)dest + read_len, 0, (size_t)(length - read_len));
        }
    } else {
        memset(dest, 0, (size_t)length);
    }

    if (s_extra_fmeta_buf) {
        uint64_t f_start = s_extra_fmeta_off;
        uint64_t f_end = s_extra_fmeta_off + s_extra_fmeta_size;
        if (offset < f_end && offset + length > f_start) {
            uint64_t o_start = (offset > f_start) ? offset : f_start;
            uint64_t o_end = (offset + length < f_end) ? (offset + length) : f_end;
            if (o_end > o_start) {
                uint64_t dest_off = o_start - offset;
                uint64_t buf_off = o_start - f_start;
                memcpy((char *)dest + dest_off, s_extra_fmeta_buf + buf_off, (size_t)(o_end - o_start));
            }
        }
    }

    for (int i = 0; i < s_extra_num_overlays; i++) {
        uint64_t v_start = s_extra_overlays[i].virtual_start;
        uint64_t v_end = v_start + s_extra_overlays[i].virtual_size;
        if (offset < v_end && offset + length > v_start) {
            uint64_t o_start = (offset > v_start) ? offset : v_start;
            uint64_t o_end = (offset + length < v_end) ? (offset + length) : v_end;
            if (o_end > o_start) {
                uint64_t dest_off = o_start - offset;
                uint64_t file_off = o_start - v_start;
                pread(s_extra_overlays[i].fd, (char *)dest + dest_off, (size_t)(o_end - o_start), (off_t)file_off);
                fprintf(stderr, "[romfs_overlay] SERVED overlay read: %s (off 0x%llx, len 0x%llx)\n",
                        s_extra_overlays[i].path, (unsigned long long)file_off, (unsigned long long)(o_end - o_start));
            }
        }
    }

    return true;
}

#ifdef __cplusplus
}
#endif
