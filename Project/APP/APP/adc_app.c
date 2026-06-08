/*
 * adc采集任务
 * 内部ADC由DMA持续更新adc_value[2]，无需搬运
 * 外部ADC(GD30AD3344 AIN0-AIN2)每100ms采一次，缓存到g_ext_adc_voltage供温度查询用
 */
#include "mcu_cmic_gd32f470vet6.h"
#include "adc_app.h"
#include "gd30ad3344.h"

extern uint16_t adc_value[2];
extern uint16_t convertarr[CONVERT_NUM];

uint16_t g_dac_val        = 0;
float    g_ext_adc_voltage = 0.0f;

void adc_task(void)
{
    static uint32_t last_ext_adc_ms = 0;
    uint32_t now = get_system_ms();

    if((now - last_ext_adc_ms) >= 100U){
        last_ext_adc_ms = now;
        g_ext_adc_voltage = GD30AD3344_AD_Read(GD30AD3344_Channel_4, GD30AD3344_PGA_0V064);
    }
}
