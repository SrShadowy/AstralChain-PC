#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdatomic.h>
#include "shim/shaderc/shaderc.h"

struct shaderc_compiler { int unused; };
struct shaderc_compile_options { int optimize; };
struct shaderc_compilation_result { shaderc_compilation_status status; char *bytes; size_t size; char *message; };

static inline const char *extra_find_glslang(void) {
    const char *path = getenv("AC_GLSLANG");
    if (path && access(path, X_OK) == 0) return path;
    if (access("tools/glslang/bin/glslang", X_OK) == 0) return "tools/glslang/bin/glslang";
    if (access("/usr/bin/glslangValidator", X_OK) == 0) return "/usr/bin/glslangValidator";
    if (access("/usr/bin/glslang", X_OK) == 0) return "/usr/bin/glslang";
    return NULL;
}

shaderc_compiler_t shaderc_compiler_initialize(void) {
    if (!extra_find_glslang()) return NULL;
    return (shaderc_compiler_t)calloc(1, sizeof(struct shaderc_compiler));
}

void shaderc_compiler_release(shaderc_compiler_t c) { free(c); }

shaderc_compile_options_t shaderc_compile_options_initialize(void) {
    return (shaderc_compile_options_t)calloc(1, sizeof(struct shaderc_compile_options));
}

void shaderc_compile_options_set_target_env(shaderc_compile_options_t o, shaderc_target_env e, unsigned v) {
    (void)o; (void)e; (void)v;
}

void shaderc_compile_options_set_optimization_level(shaderc_compile_options_t o, shaderc_optimization_level l) {
    if (o) o->optimize = (l != shaderc_optimization_level_zero);
}

void shaderc_compile_options_release(shaderc_compile_options_t o) { free(o); }

static shaderc_compilation_result_t shaderc_posix_failure(const char *message) {
    shaderc_compilation_result_t r = (shaderc_compilation_result_t)calloc(1, sizeof *r);
    if (!r) return NULL;
    r->status = shaderc_compilation_status_compilation_error;
    r->message = strdup(message ? message : "glslang shim failure");
    return r;
}

static int run_glslang_posix(const char *glslang, const char *source, size_t size,
                            const char *stage, const char *spv, int optimize, char **log) {
    static atomic_long sub_cnt = 0;
    long cid = atomic_fetch_add(&sub_cnt, 1);
    const char *temp = getenv("TMPDIR");
    if (!temp || !*temp) temp = "/tmp";

    char src_file[4096], log_file[4096];
    snprintf(src_file, sizeof src_file, "%s/ac_src_%d_%ld.%s", temp, (int)getpid(), cid, stage);
    snprintf(log_file, sizeof log_file, "%s/ac_log_%d_%ld.txt", temp, (int)getpid(), cid);

    FILE *sf = fopen(src_file, "wb");
    if (!sf) return -1;
    fwrite(source, 1, size, sf);
    fclose(sf);

    pid_t pid = fork();
    if (pid < 0) {
        unlink(src_file);
        return -1;
    }
    if (pid == 0) {
        int log_fd = open(log_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (log_fd >= 0) {
            dup2(log_fd, STDOUT_FILENO);
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);
        }
        execlp(glslang, glslang, "-V", "--target-env", "vulkan1.3",
               optimize ? "-Os" : "-Od", "-S", stage, "-o", spv, src_file, (char*)NULL);
        _exit(127);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    unlink(src_file);

    if (log) {
        *log = NULL;
        FILE *lf = fopen(log_file, "rb");
        if (lf) {
            fseek(lf, 0, SEEK_END);
            long sz = ftell(lf);
            fseek(lf, 0, SEEK_SET);
            if (sz > 0) {
                char *lbuf = (char*)malloc((size_t)sz + 1);
                if (lbuf && fread(lbuf, 1, (size_t)sz, lf) == (size_t)sz) {
                    lbuf[sz] = '\0';
                    *log = lbuf;
                } else {
                    free(lbuf);
                }
            }
            fclose(lf);
        }
    }
    unlink(log_file);

    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

shaderc_compilation_result_t shaderc_compile_into_spv(shaderc_compiler_t c, const char *source, size_t size,
    shaderc_shader_kind kind, const char *name, const char *entry, shaderc_compile_options_t o) {
    (void)c; (void)name; (void)entry;
    static atomic_long counter = 0;
    const char *glslang = extra_find_glslang();
    if (!glslang) return shaderc_posix_failure("glslang not found or not executable");

    const char *temp = getenv("TMPDIR");
    if (!temp || !*temp) temp = "/tmp";

    const char *stage = (kind == shaderc_vertex_shader) ? "vert" : (kind == shaderc_fragment_shader) ? "frag" : "comp";
    char spv[4096];
    snprintf(spv, sizeof spv, "%s/ac_glslang_%d_%ld.spv", temp, (int)getpid(), atomic_fetch_add(&counter, 1));

    shaderc_compilation_result_t r = NULL;
    for (int attempt = 0; attempt < 3 && !r; attempt++) {
        if (attempt) usleep(50000 * attempt);
        char *log = NULL;
        int code = run_glslang_posix(glslang, source, size, stage, spv, o && o->optimize, &log);
        if (code != 0) {
            if (attempt == 2 || (code > 0 && !strstr(log ? log : "", "cannot open"))) {
                r = shaderc_posix_failure(log);
            }
            free(log);
            continue;
        }
        free(log);

        FILE *f = fopen(spv, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (sz > 0 && (sz % 4) == 0) {
                char *bytes = (char*)malloc((size_t)sz);
                if (bytes && fread(bytes, 1, (size_t)sz, f) == (size_t)sz) {
                    r = (shaderc_compilation_result_t)calloc(1, sizeof *r);
                    if (r) {
                        r->status = shaderc_compilation_status_success;
                        r->bytes = bytes;
                        r->size = (size_t)sz;
                        static unsigned s_sh_log = 0;
                        if (s_sh_log++ < 20) printf("[shaders] stage %s: compiled %zu bytes SPIR-V\n", stage, (size_t)sz);
                    } else {
                        free(bytes);
                    }
                } else {
                    free(bytes);
                }
            }
            fclose(f);
        }
    }
    unlink(spv);
    return r ? r : shaderc_posix_failure("cannot start glslang or invalid output");
}

shaderc_compilation_status shaderc_result_get_compilation_status(shaderc_compilation_result_t r) {
    return r ? r->status : shaderc_compilation_status_compilation_error;
}
const char *shaderc_result_get_error_message(shaderc_compilation_result_t r) {
    return (r && r->message) ? r->message : "";
}
size_t shaderc_result_get_length(shaderc_compilation_result_t r) {
    return r ? r->size : 0;
}
const char *shaderc_result_get_bytes(shaderc_compilation_result_t r) {
    return r ? r->bytes : NULL;
}
void shaderc_result_release(shaderc_compilation_result_t r) {
    if (!r) return;
    free(r->bytes);
    free(r->message);
    free(r);
}
