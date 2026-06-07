#ifndef __LED_APP_H
#define __LED_APP_H

#include <stdint.h>

/* 6个led的状态，1=亮 */
extern uint8_t g_led[6];

void led_task(void);

#endif
