/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2026/04/29
* Note:
*/
#include "mcu_cmic_gd32f470vet6.h"
#include "bl_core.h"

int main(void)
{
    systick_config();
    bsp_usart_init();
    bsp_oled_init();
    OLED_Init();
    OLED_ShowStr(0, 0, "2026055095      ", 8);
    OLED_ShowStr(0, 1, "Bootloader      ", 8);

    bootloader_run();

    while(1) {
    }
}
