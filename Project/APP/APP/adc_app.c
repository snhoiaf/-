/*
 * adc采集任务
 * 从dma缓冲搬到本地，顺便联动dac做个回环
 */
#include "mcu_cmic_gd32f470vet6.h"
#include "adc_app.h"

extern uint16_t adc_value[2];
extern uint16_t convertarr[CONVERT_NUM];

uint16_t g_dac_val = 0;

void adc_task(void)
{
    /* 目前把ch10的值丢给dac输出，测试用 */
    g_dac_val = adc_value[0];
    convertarr[0] = g_dac_val;
}
