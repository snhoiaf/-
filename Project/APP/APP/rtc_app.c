/*
 * rtc任务
 * 仅更新时间缓存，OLED显示由oled_task统一维护双行内容
 */
#include "mcu_cmic_gd32f470vet6.h"
#include "rtc_app.h"

extern rtc_parameter_struct rtc_initpara;

void rtc_task(void)
{
    rtc_current_time_get(&rtc_initpara);
}
