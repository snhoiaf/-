#ifndef __USART_APP_H
#define __USART_APP_H

#include <stdint.h>

int  my_printf(uint32_t usart, const char *fmt, ...);
void uart_task(void);
void ota_reset_state(void);
uint8_t sampling_is_active(void);
void debug_uart_frame_callback(const uint8_t *d, uint16_t len);
void usart1_frame_callback(const uint8_t *d, uint16_t len);

#endif
