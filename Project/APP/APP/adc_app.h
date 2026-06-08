#ifndef __ADC_APP_H
#define __ADC_APP_H

#include <stdint.h>

extern uint16_t g_dac_val;
extern float    g_ext_adc_voltage; /* GD30AD3344 CH4 缓存值（V），100ms刷新 */

void adc_task(void);

#endif
