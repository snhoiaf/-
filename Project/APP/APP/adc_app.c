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
    /* ADC采样由DMA更新，DAC输出由协议命令/持久化配置控制 */
    (void)adc_value;
    (void)convertarr;
}
