/*
 * BootLoader任务调度模块
 * 基于时间戳的轮询调度器
 */
#include "mcu_cmic_gd32f470vet6.h"

/* 任务条目数量 */
static uint8_t job_cnt;

/* 任务描述结构 */
typedef struct {
    void (*entry)(void);
    uint32_t interval;
    uint32_t timestamp;
} job_desc_t;

/* 静态任务表 */
static job_desc_t job_list[] =
{
     {led_task,  1,    0}
    ,{adc_task,  100,  0}
    ,{oled_task, 10,   0}
    ,{btn_task,  5,    0}
    ,{uart_task, 5,    0}
    ,{rtc_task,  500,  0}
};

/*
 * 初始化调度器，计算任务总数
 */
void scheduler_init(void)
{
    job_cnt = sizeof(job_list) / sizeof(job_desc_t);
}

/*
 * 调度器主循环，检查并执行到期任务
 */
void scheduler_run(void)
{
    uint8_t idx;
    uint32_t current;

    for(idx = 0; idx < job_cnt; idx++){
        current = get_system_ms();
        if(current >= job_list[idx].interval + job_list[idx].timestamp){
            job_list[idx].timestamp = current;
            job_list[idx].entry();
        }
    }
}
