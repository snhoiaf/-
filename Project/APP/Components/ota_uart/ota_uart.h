#ifndef __OTA_UART_H
#define __OTA_UART_H

#include <stdint.h>

void ota_reset(void);
void ota_feed(const uint8_t *data, uint32_t len);
void ota_poll(void);

#endif
