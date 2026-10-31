// [extra]
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "../../exec_internal.h"

typedef struct WaitRecord {
    const char *why;
    uint64_t ns, n;
} WaitRecord;

extern WaitRecord wait_tab[12];
extern pthread_mutex_t wait_tab_lock;
extern uint64_t stat_counters_gpu;
extern bool pace_slow;
extern uint64_t stat_pace_over;

void submission_init(void);
void handle_reset_counter(uint32_t type);
void handle_report_counter(uint32_t type, uint64_t *out);
