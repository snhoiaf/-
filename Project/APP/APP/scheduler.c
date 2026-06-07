/*
 * 任务调度器
 * 轮询方式，按时间片执行
 */
#include "mcu_cmic_gd32f470vet6.h"
#include "scheduler.h"

#define MAX_JOBS    16

typedef struct {
    task_fn     run;
    uint32_t    period;
    uint32_t    prev;
} job_t;

static job_t jobs[MAX_JOBS];
static uint8_t cnt;

void sched_init(void)
{
    cnt = 0;
    for(uint8_t i = 0; i < MAX_JOBS; i++){
        jobs[i].run    = 0;
        jobs[i].period = 0;
        jobs[i].prev   = 0;
    }
}

int sched_bind(task_fn fn, uint32_t ms)
{
    if(cnt >= MAX_JOBS || !fn) return -1;
    jobs[cnt].run    = fn;
    jobs[cnt].period = ms;
    jobs[cnt].prev   = 0;
    cnt++;
    return 0;
}

void sched_run(void)
{
    uint32_t now;
    uint8_t idx;

    now = get_system_ms();
    for(idx = 0; idx < cnt; idx++){
        if((now - jobs[idx].prev) >= jobs[idx].period){
            jobs[idx].prev = now;
            jobs[idx].run();
        }
    }
}
