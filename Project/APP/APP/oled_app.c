/*
 * oled显示任务
 * 固定双行显示: 队伍编号 / 运行状态
 */
#include "mcu_cmic_gd32f470vet6.h"
#include "oled_app.h"
#include "usart_app.h"

#define OLED_TEAM_ID        "2026055095"
#define OLED_STATE_IDLE     "IDLE"
#define OLED_STATE_SAMPLE   "AutoSample"
#define OLED_LINE_WIDTH     16

int oled_printf(uint8_t x, uint8_t y, const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    OLED_ShowStr(x, y, buf, 8);
    return n;
}

void oled_task(void)
{
    static uint8_t last_state = 0xFFU;
    static uint8_t first_show = 1U;
    uint8_t active = sampling_is_active();
    const char *state = active ? OLED_STATE_SAMPLE : OLED_STATE_IDLE;

    if (first_show) {
        OLED_Clear();
        oled_printf(0, 0, "%-*s", OLED_LINE_WIDTH, OLED_TEAM_ID);
        first_show = 0U;
    }

    if (last_state != active) {
        oled_printf(0, 1, "%-*s", OLED_LINE_WIDTH, state);
        last_state = active;
    }
}
