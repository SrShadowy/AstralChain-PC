// [extra]
#include <stdbool.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <SDL3/SDL_cpuinfo.h>
#include <SDL3/SDL_timer.h>
#include "../../exec_internal.h"

// ================================================================== worker threads (shader / pipeline builds)
// First-use compiles run off the render thread; draws needing them are skipped until ready (as on Metal).
// LARECOMP_SYNC_SHADERS=1 builds on first use, blocking (deterministic tests).
typedef struct Job { void (*fn)(void *); void *arg; struct Job *next; } Job;
static Job *jobs_head, *jobs_tail;
static pthread_mutex_t jobs_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t jobs_cond = PTHREAD_COND_INITIALIZER;
// Low priority (pipeline warm-up): taken only when no other job waits, by at most half of the workers at a time.
static Job *ljobs_head, *ljobs_tail;
static int ljobs_running, ljobs_max = 2;

static void *worker(void *a) {
    (void)a;
    for (;;) {
        pthread_mutex_lock(&jobs_lock);
        while (!jobs_head && !(ljobs_head && ljobs_running < ljobs_max)) pthread_cond_wait(&jobs_cond, &jobs_lock);
        Job *j;
        bool low = !jobs_head;
        if (!low) {
            j = jobs_head;
            jobs_head = j->next;
            if (!jobs_head) jobs_tail = NULL;
        } else {
            j = ljobs_head;
            ljobs_head = j->next;
            if (!ljobs_head) ljobs_tail = NULL;
            ljobs_running++;
        }
        pthread_mutex_unlock(&jobs_lock);
        j->fn(j->arg);
        free(j);
        if (low) {
            pthread_mutex_lock(&jobs_lock);
            ljobs_running--;
            if (ljobs_head) pthread_cond_signal(&jobs_cond);
            pthread_mutex_unlock(&jobs_lock);
        }
    }
    return NULL;
}

bool sync_shaders(void) {
    static int v = -1;
    if (v < 0) v = getenv("LARECOMP_SYNC_SHADERS") != NULL;
    return v;
}

// Shader translation: one dedicated thread (the translator isn't re-entrant - translate.c's RSC2 string ring - and
// translating on the game thread cost up to ~90 ms per frame on entering an area). Compiles go on to the workers.
pthread_mutex_t translate_lock = PTHREAD_MUTEX_INITIALIZER;
static Job *tjobs_head, *tjobs_tail;
static pthread_mutex_t tjobs_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t tjobs_cond = PTHREAD_COND_INITIALIZER;

static void *translator(void *a) {
    (void)a;
    for (;;) {
        pthread_mutex_lock(&tjobs_lock);
        while (!tjobs_head) pthread_cond_wait(&tjobs_cond, &tjobs_lock);
        Job *j = tjobs_head;
        tjobs_head = j->next;
        if (!tjobs_head) tjobs_tail = NULL;
        pthread_mutex_unlock(&tjobs_lock);
        j->fn(j->arg);
        free(j);
    }
    return NULL;
}

void run_translate(void (*fn)(void *), void *arg) {
    if (sync_shaders()) { fn(arg); return; }
    static bool started;
    if (!started) { started = true; pthread_t t; pthread_create(&t, NULL, translator, NULL); }
    Job *j = malloc(sizeof *j);
    j->fn = fn; j->arg = arg; j->next = NULL;
    pthread_mutex_lock(&tjobs_lock);
    if (tjobs_tail) tjobs_tail->next = j; else tjobs_head = j;
    tjobs_tail = j;
    pthread_cond_signal(&tjobs_cond);
    pthread_mutex_unlock(&tjobs_lock);
}

void workers_start(void) {
    static bool started;
    if (!started) {
        started = true;
        // first entry into an area builds dozens of pipelines at once: use most of the CPU (draws wait for them)
        int n = SDL_GetNumLogicalCPUCores() - 4;
        if (n < 4) n = 4;
        if (n > 16) n = 16;
        ljobs_max = n / 2;
        for (int i = 0; i < n; i++) { pthread_t t; pthread_create(&t, NULL, worker, NULL); }
    }
}

void run_job(void (*fn)(void *), void *arg) {
    if (sync_shaders()) { fn(arg); return; }
    workers_start();
    Job *j = malloc(sizeof *j);
    j->fn = fn; j->arg = arg; j->next = NULL;
    pthread_mutex_lock(&jobs_lock);
    if (jobs_tail) jobs_tail->next = j; else jobs_head = j;
    jobs_tail = j;
    pthread_cond_signal(&jobs_cond);
    pthread_mutex_unlock(&jobs_lock);
}

void run_job_low(void (*fn)(void *), void *arg) {
    workers_start();
    Job *j = malloc(sizeof *j);
    j->fn = fn; j->arg = arg; j->next = NULL;
    pthread_mutex_lock(&jobs_lock);
    if (ljobs_tail) ljobs_tail->next = j; else ljobs_head = j;
    ljobs_tail = j;
    pthread_cond_signal(&jobs_cond);
    pthread_mutex_unlock(&jobs_lock);
}

// (Astral Chain) a queued job a draw now waits for moves to the front of its queue: warm-up jobs (program warm-up
// below) must not delay the builds the current frame needs.
static void promote_job(Job **head, Job **tail, pthread_mutex_t *lock, void *arg) {
    pthread_mutex_lock(lock);
    for (Job *prev = NULL, *j = *head; j; prev = j, j = j->next) {
        if (j->arg != arg) continue;
        if (prev) {
            prev->next = j->next;
            if (*tail == j) *tail = prev;
            j->next = *head;
            *head = j;
        }
        break;
    }
    pthread_mutex_unlock(lock);
}

void promote_worker_job(void *arg) {
    promote_job(&jobs_head, &jobs_tail, &jobs_lock, arg);
}

void promote_translate_job(void *arg) {
    promote_job(&tjobs_head, &tjobs_tail, &tjobs_lock, arg);
}

bool skip_unbuilt(void) {
    static int skip = -1;
    if (skip < 0) skip = getenv("LARECOMP_SKIP_UNBUILT") != NULL;
    return skip;
}

void wait_built(_Atomic int *state) {
    if (skip_unbuilt() || atomic_load(state) != 0) return;
    uint64_t t0 = SDL_GetTicksNS(), now = t0;
    while (atomic_load(state) == 0 && now - t0 < 3000000000ull) {
        if (now - t0 < 20000000ull) sched_yield(); else SDL_Delay(1);
        now = SDL_GetTicksNS();
    }
    cum_built_ns += SDL_GetTicksNS() - t0;
}
// [extra]
