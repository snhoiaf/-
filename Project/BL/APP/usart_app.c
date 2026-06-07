/*
 * BootLoader串口模块
 * 调试输出 + 帧接收处理
 */
#include "mcu_cmic_gd32f470vet6.h"

__IO uint16_t uart_tx_idx = 0;
__IO uint8_t uart_rx_ready = 0;
uint8_t rx_frame_buf[256] = {0};

/*
 * 格式化串口输出函数
 */
int my_printf(uint32_t port, const char *fmt, ...)
{
    char tmp[256];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);

    if(port == USART1){
        gpio_bit_set(GPIOE, GPIO_PIN_8);
        delay_1ms(1);
    }

    for(uart_tx_idx = 0; uart_tx_idx < n; uart_tx_idx++){
        usart_data_transmit(port, tmp[uart_tx_idx]);
        while(RESET == usart_flag_get(port, USART_FLAG_TBE));
    }
    while(RESET == usart_flag_get(port, USART_FLAG_TC));

    if(port == USART1){
        delay_1ms(1);
        gpio_bit_reset(GPIOE, GPIO_PIN_8);
    }

    return n;
}

/*
 * 串口任务处理
 */
void uart_task(void)
{
    if(!uart_rx_ready) return;

    my_printf(DEBUG_USART, "%s", rx_frame_buf);
    memset(rx_frame_buf, 0, sizeof(rx_frame_buf));
    uart_rx_ready = 0;
}
