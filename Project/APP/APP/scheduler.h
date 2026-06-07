#ifndef __SCHEDULER_H
#define __SCHEDULER_H

#include <stdint.h>

/* 最多挂16个任务 */
#define SCHED_MAX_TASK  16

typedef void (*task_fn)(void);

typedef struct {
    task_fn     handler;
    uint32_t    interval;
    uint32_t    stamp;
} sched_node_t;

void sched_init(void);
int  sched_bind(task_fn fn, uint32_t ms);
void sched_run(void);

#endif
