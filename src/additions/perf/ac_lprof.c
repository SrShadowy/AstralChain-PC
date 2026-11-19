#define _GNU_SOURCE
#include <dirent.h>
#include <execinfo.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#define LPROF_SIG            (SIGRTMIN + 2)
#define LPROF_DEPTH          12
#define LPROF_MAX_SAMPLES    (1u << 20)
#define LPROF_MAX_TIDS       512
#define LPROF_TASK_REFRESH   64u
#define LPROF_MAX_MISSES     3
#define LPROF_ACK_TIMEOUT_NS 150000L
#define LPROF_ROUND_US       5000u
#define LPROF_PATH_MAX       512
#define LPROF_COMM_MAX       32
#define LPROF_NS_PER_SEC     1000000000LL

typedef struct {
    int tid;
    int count;
    char comm[LPROF_COMM_MAX];
    void *pc[LPROF_DEPTH];
} LprofSample;

typedef struct {
    char out_path[LPROF_PATH_MAX];
    double start_sec;
    double duration_sec;
} LprofConfig;

static LprofSample *g_samples;
static volatile uint32_t g_sample_count;
static volatile int g_sample_ack;
static int g_started;

static uint64_t monotonic_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * (uint64_t)LPROF_NS_PER_SEC + (uint64_t)ts.tv_nsec;
}

static void sleep_ms(double ms) {
    if (ms <= 0.0) return;
    uint64_t ns = (uint64_t)(ms * 1000000.0);
    struct timespec req;
    req.tv_sec = (time_t)(ns / (uint64_t)LPROF_NS_PER_SEC);
    req.tv_nsec = (long)(ns % (uint64_t)LPROF_NS_PER_SEC);
    while (nanosleep(&req, &req) == -1) {
    }
}

static void read_tid_comm(int tid, char out[LPROF_COMM_MAX]) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/self/task/%d/comm", tid);
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(out, LPROF_COMM_MAX, "-");
        return;
    }
    if (!fgets(out, LPROF_COMM_MAX, f)) {
        snprintf(out, LPROF_COMM_MAX, "-");
    }
    fclose(f);
    for (size_t i = 0; out[i]; i++) {
        if (out[i] == '\n' || out[i] == '\r') {
            out[i] = '\0';
            break;
        }
        if (out[i] == ' ') out[i] = '_';
    }
    if (!out[0]) snprintf(out, LPROF_COMM_MAX, "-");
}

static void lprof_signal_handler(int sig, siginfo_t *si, void *ucv) {
    (void)sig;
    (void)si;
    uint32_t idx = __atomic_fetch_add(&g_sample_count, 1u, __ATOMIC_RELAXED);
    if (idx < LPROF_MAX_SAMPLES) {
        LprofSample *s = &g_samples[idx];
        void *tmp[LPROF_DEPTH + 3];
        int k = backtrace(tmp, LPROF_DEPTH + 3);
        int j = 0;
        const ucontext_t *uc = (const ucontext_t *)ucv;
        s->tid = (int)syscall(SYS_gettid);
        s->pc[j++] = (void *)(uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
        for (int m = 2; m < k && j < LPROF_DEPTH; m++) {
            s->pc[j++] = tmp[m];
        }
        s->count = j;
    }
    __atomic_store_n(&g_sample_ack, 1, __ATOMIC_RELEASE);
}

static int g_blocked_tids[LPROF_MAX_TIDS];
static int g_blocked_count;

static int is_tid_blocked(int tid) {
    for (int i = 0; i < g_blocked_count; i++) {
        if (g_blocked_tids[i] == tid) return 1;
    }
    return 0;
}

static void mark_tid_blocked(int tid) {
    if (!is_tid_blocked(tid) && g_blocked_count < LPROF_MAX_TIDS) {
        g_blocked_tids[g_blocked_count++] = tid;
    }
}

static int refresh_tids(long self_tid, int tids[LPROF_MAX_TIDS], int miss[LPROF_MAX_TIDS],
                        char comms[LPROF_MAX_TIDS][LPROF_COMM_MAX]) {
    DIR *d = opendir("/proc/self/task");
    if (!d) return 0;
    int nt = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && nt < LPROF_MAX_TIDS) {
        long t = atol(ent->d_name);
        if (t <= 0 || t == self_tid || is_tid_blocked((int)t)) continue;
        tids[nt] = (int)t;
        miss[nt] = 0;
        read_tid_comm((int)t, comms[nt]);
        nt++;
    }
    closedir(d);
    return nt;
}

static void write_lprof_report(const char *path, double secs, uint64_t rounds, uint32_t count) {
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "ac_lprof: failed to open %s for writing\n", path);
        return;
    }
    fprintf(f, "# ac_lprof %.1f s, %u samples, %llu rounds\n",
            secs, count, (unsigned long long)rounds);
    for (uint32_t i = 0; i < count; i++) {
        fprintf(f, "S %d %s", g_samples[i].tid, g_samples[i].comm[0] ? g_samples[i].comm : "-");
        for (int j = 0; j < g_samples[i].count; j++) {
            fprintf(f, " %llx", (unsigned long long)(uintptr_t)g_samples[i].pc[j]);
        }
        fputc('\n', f);
    }
    FILE *m = fopen("/proc/self/maps", "r");
    char line[600];
    while (m && fgets(line, sizeof(line), m)) {
        fprintf(f, "M %s", line);
    }
    if (m) fclose(m);
    fclose(f);
    printf("ac_lprof: %u samples written to %s\n", count, path);
    fflush(stdout);
}

static void *lprof_worker(void *arg) {
    LprofConfig cfg = *(LprofConfig *)arg;
    free(arg);

    void *warm[LPROF_DEPTH];
    (void)backtrace(warm, LPROF_DEPTH);

    g_samples = (LprofSample *)calloc(LPROF_MAX_SAMPLES, sizeof(LprofSample));
    if (!g_samples) return NULL;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = lprof_signal_handler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(LPROF_SIG, &sa, NULL);

    sleep_ms(cfg.start_sec * 1000.0);

    long pid = (long)getpid();
    long self_tid = (long)syscall(SYS_gettid);
    printf("ac_lprof: sampling threads for %.1f s -> %s\n", cfg.duration_sec, cfg.out_path);
    fflush(stdout);

    int tids[LPROF_MAX_TIDS];
    int miss[LPROF_MAX_TIDS];
    char comms[LPROF_MAX_TIDS][LPROF_COMM_MAX];
    int nt = 0;
    uint64_t rounds = 0;
    uint64_t t0 = monotonic_ns();
    uint64_t dur_ns = (uint64_t)(cfg.duration_sec * (double)LPROF_NS_PER_SEC);

    while (monotonic_ns() - t0 < dur_ns && __atomic_load_n(&g_sample_count, __ATOMIC_RELAXED) < LPROF_MAX_SAMPLES) {
        if ((rounds++ % LPROF_TASK_REFRESH) == 0) {
            nt = refresh_tids(self_tid, tids, miss, comms);
        }
        for (int i = 0; i < nt; i++) {
            if (miss[i] >= LPROF_MAX_MISSES) continue;
            uint32_t before = __atomic_load_n(&g_sample_count, __ATOMIC_RELAXED);
            __atomic_store_n(&g_sample_ack, 0, __ATOMIC_RELEASE);
            if (syscall(SYS_tgkill, pid, (long)tids[i], LPROF_SIG) != 0) {
                miss[i] = LPROF_MAX_MISSES;
                continue;
            }
            uint64_t wait_t0 = monotonic_ns();
            for (;;) {
                if (__atomic_load_n(&g_sample_ack, __ATOMIC_ACQUIRE)) {
                    if (before < LPROF_MAX_SAMPLES) {
                        memcpy(g_samples[before].comm, comms[i], LPROF_COMM_MAX);
                    }
                    miss[i] = 0;
                    break;
                }
                if ((long)(monotonic_ns() - wait_t0) > LPROF_ACK_TIMEOUT_NS) {
                    miss[i]++;
                    if (miss[i] >= LPROF_MAX_MISSES) mark_tid_blocked(tids[i]);
                    break;
                }
                sched_yield();
            }
        }
        usleep(LPROF_ROUND_US);
    }

    uint32_t recorded = __atomic_load_n(&g_sample_count, __ATOMIC_RELAXED);
    if (recorded > LPROF_MAX_SAMPLES) recorded = LPROF_MAX_SAMPLES;
    write_lprof_report(cfg.out_path, cfg.duration_sec, rounds, recorded);
    return NULL;
}

void ac_lprof_start(void) {
    if (__atomic_exchange_n(&g_started, 1, __ATOMIC_ACQ_REL)) return;
    const char *spec = getenv("AC_LPROF");
    if (!spec || !*spec) return;

    LprofConfig *cfg = (LprofConfig *)calloc(1, sizeof(LprofConfig));
    if (!cfg) return;
    snprintf(cfg->out_path, sizeof(cfg->out_path), "ac_lprof.txt");

    if (sscanf(spec, "%lf,%lf", &cfg->start_sec, &cfg->duration_sec) != 2 || cfg->duration_sec <= 0.0) {
        fprintf(stderr, "ac_lprof: usage AC_LPROF=<start_s>,<duration_s>[,<output_file>]\n");
        free(cfg);
        return;
    }
    const char *c1 = strchr(spec, ',');
    const char *c2 = c1 ? strchr(c1 + 1, ',') : NULL;
    if (c2 && c2[1]) {
        snprintf(cfg->out_path, sizeof(cfg->out_path), "%s", c2 + 1);
    }

    pthread_t th;
    if (pthread_create(&th, NULL, lprof_worker, cfg) == 0) {
        pthread_detach(th);
    } else {
        free(cfg);
    }
}
