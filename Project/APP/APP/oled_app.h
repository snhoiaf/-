#ifndef __OLED_APP_H
#define __OLED_APP_H

#include <stdint.h>

int oled_printf(uint8_t x, uint8_t y, const char *fmt, ...);
void oled_task(void);

#endif
