/*
 * V2板 主程序
 * 2026-05 改写
 *
 * 初始化顺序:
 *   系统时钟 → 串口 → 外设 → 调度器 → 主循环
 */
#include "mcu_cmic_gd32f470vet6.h"
#include "scheduler.h"
#include "led_app.h"
#include "adc_app.h"
#include "oled_app.h"
#include "btn_app.h"
#include "rtc_app.h"
#include "usart_app.h"
#include "sd_app.h"
#include "bl_partition.h"

static void sys_init(void)
{
    SCB->VTOR = BL_APP1_START_ADDR;

    systick_config();
    init_cycle_counter(false);
    delay_ms(200);   /* swio复用gpio时留点时间 */

    bsp_led_init();
    bsp_btn_init();
    bsp_oled_init();
    bsp_gd25qxx_init();
    bsp_usart_all_init();

    my_printf(DEBUG_USART, "\r\n=== V2 APP boot v2.0 OTA TEST ===\r\n");

    bsp_gd30ad3344_init();
    bsp_adc_init();
    bsp_dac_init();
    bsp_rtc_init();

    sd_fatfs_init();
    btn_init();

    OLED_Init();
    my_printf(DEBUG_USART, "OLED ok\r\n");

    test_spi_flash();

#if SD_FATFS_DEMO_ENABLE
    sd_fatfs_test();
#else
    my_printf(DEBUG_USART, "SD test skipped\r\n");
#endif

    ota_reset_state();
}

static void task_bind(void)
{
    sched_init();

    sched_bind(led_task,  2);
    sched_bind(adc_task,  100);
    sched_bind(oled_task, 10);
    sched_bind(btn_task,  5);
    sched_bind(uart_task, 5);
}

int main(void)
{
    sys_init();
    task_bind();

    my_printf(DEBUG_USART, "entering loop\r\n");

    for(;;){
        sched_run();
    }
}

/* printf重定向 */
#ifdef GD_ECLIPSE_GCC
int __io_putchar(int ch)
{
    usart_data_transmit(DEBUG_USART, (uint8_t)ch);
    while(RESET == usart_flag_get(DEBUG_USART, USART_FLAG_TBE));
    return ch;
}
#else
int fputc(int ch, FILE *f)
{
    usart_data_transmit(DEBUG_USART, (uint8_t)ch);
    while(RESET == usart_flag_get(DEBUG_USART, USART_FLAG_TBE));
    return ch;
}
#endif
