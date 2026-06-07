/*
 * led任务
 * 状态数组驱动，有变化才写gpio，省点开销
 */
#include "mcu_cmic_gd32f470vet6.h"
#include "led_app.h"
#include "usart_app.h"

#define SYS_LED_INDEX       0U  /* LED1: 系统状态指示灯 */
#define SAMPLE_LED_INDEX    1U  /* LED2: 采集工作指示灯 */
#define SYS_LED_BLINK_MS    1000U

uint8_t g_led[6] = {0, 0, 0, 0, 0, 0};

static void led_status_update(void)
{
    static uint32_t last_ms = 0;
    static uint8_t sys_state = 1;
    uint32_t now = (uint32_t)get_system_ms();

    if((now - last_ms) >= SYS_LED_BLINK_MS){
        last_ms = now;
        sys_state ^= 1U;
    }

    g_led[SYS_LED_INDEX] = sys_state;
    g_led[SAMPLE_LED_INDEX] = sampling_is_active() ? 1U : 0U;
}

void led_task(void)
{
    uint8_t bits = 0, i;
    static uint8_t cache = 0xFF;

    led_status_update();

    for(i = 0; i < 6; i++){
        if(g_led[i]) bits |= (1 << i);
    }
    if(bits == cache) return;
    cache = bits;

    LED1_SET(bits & 0x01);
    LED2_SET(bits & 0x02);
    LED3_SET(bits & 0x04);
    LED4_SET(bits & 0x08);
    LED5_SET(bits & 0x10);
    LED6_SET(bits & 0x20);
}
