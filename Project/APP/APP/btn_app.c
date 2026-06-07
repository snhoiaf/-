/*
 * 按键任务
 * 用ebtn库做消抖，单击翻转对应led
 * 2026-05 改写
 */
#include "mcu_cmic_gd32f470vet6.h"
#include "btn_app.h"
#include "led_app.h"

enum { K1=0, K2, K3, K4, K5, K6, KW, K_NUM };

static const ebtn_btn_param_t kpar = EBTN_PARAMS_INIT(20, 0, 20, 1000, 0, 1000, 10);
static ebtn_btn_t btns[K_NUM];

static uint8_t pin_read(struct ebtn_btn *b)
{
    switch(b->key_id){
        case K1: return !KEY1_READ;
        case K2: return !KEY2_READ;
        case K3: return !KEY3_READ;
        case K4: return !KEY4_READ;
        case K5: return !KEY5_READ;
        case K6: return !KEY6_READ;
        case KW: return !KEYW_READ;
        default: return 0;
    }
}

static void pin_event(struct ebtn_btn *b, ebtn_evt_t evt)
{
    if(evt != EBTN_EVT_ONCLICK) return;

    /* LED1/LED2保留给系统/采集指示，按键只操作LED3~LED6 */
    if(b->key_id < 4)
        g_led[b->key_id + 2] ^= 1;
    else
        g_led[5] ^= 1;
}

void btn_init(void)
{
    uint8_t i;
    for(i = 0; i < K_NUM; i++){
        btns[i].key_id = i;
        btns[i].param  = &kpar;
    }
    ebtn_init(btns, K_NUM, NULL, 0, pin_read, pin_event);
}

void btn_task(void)
{
    ebtn_process(get_system_ms());
}
