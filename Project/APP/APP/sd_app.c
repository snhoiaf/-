/*
 * SD卡功能模块
 * 初始化 + 读写测试 + 长文件名
 */
#include "mcu_cmic_gd32f470vet6.h"
#include "sd_app.h"

static FATFS fatfs;
static FIL   fp;

static int buf_cmp(const uint8_t *a, const uint8_t *b, uint16_t sz)
{
    while(sz--){
        if(*a++ != *b++) return 0;
    }
    return 1;
}

void sd_fatfs_init(void)
{
    nvic_irq_enable(SDIO_IRQn, 0, 0);
}

static void print_card_info(void)
{
    sd_card_info_struct info;
    if(SD_OK == sd_card_information_get(&info)){
        my_printf(DEBUG_USART, "SD: %luMB\r\n", sd_card_capacity_get() / 1024);
    } else {
        my_printf(DEBUG_USART, "SD: read info fail\r\n");
    }
}

static void lfn_test(void)
{
    static const char path[] = "0:/log_2026_dev_gd32f470_ch01_run0001_longname.txt";
    static const char data[] = "LFN_OK";
    FIL lf;
    UINT rw;
    uint8_t rd[32];

    if(FR_OK != f_open(&lf, path, FA_CREATE_ALWAYS | FA_WRITE)){
        my_printf(DEBUG_USART, "SD: lfn write fail\r\n");
        return;
    }
    f_write(&lf, data, (UINT)strlen(data), &rw);
    f_close(&lf);

    if(FR_OK != f_open(&lf, path, FA_OPEN_EXISTING | FA_READ)){
        my_printf(DEBUG_USART, "SD: lfn read fail\r\n");
        return;
    }
    memset(rd, 0, sizeof(rd));
    f_read(&lf, rd, sizeof(rd) - 1, &rw);
    f_close(&lf);

    if(rw == strlen(data) && 0 == memcmp(rd, data, rw))
        my_printf(DEBUG_USART, "SD LFN: PASS\r\n");
    else
        my_printf(DEBUG_USART, "SD LFN: FAIL\r\n");
}

void sd_fatfs_test(void)
{
    UINT bw, br;
    DSTATUS ds;
    uint8_t retry;
    uint8_t wbuf[128], rbuf[128];

    retry = 5;
    do {
        ds = disk_initialize(0);
    } while(ds != 0 && --retry);

    print_card_info();
    my_printf(DEBUG_USART, "SD init: %d\r\n", ds);

    if(FR_OK != f_mount(0, &fatfs)){
        my_printf(DEBUG_USART, "SD mount fail\r\n");
        return;
    }
    my_printf(DEBUG_USART, "SD mount OK\r\n");

    if(FR_OK == f_open(&fp, "0:/TEST.TXT", FA_CREATE_ALWAYS | FA_WRITE)){
        sprintf((char*)wbuf, "HELLO MCUSTUDIO");
        f_write(&fp, wbuf, (UINT)strlen((char*)wbuf), &bw);
        f_close(&fp);
        my_printf(DEBUG_USART, "write: %u bytes\r\n", bw);
    }

    if(FR_OK == f_open(&fp, "0:/TEST.TXT", FA_OPEN_EXISTING | FA_READ)){
        memset(rbuf, 0, sizeof(rbuf));
        f_read(&fp, rbuf, sizeof(rbuf) - 1, &br);
        f_close(&fp);

        if(br == strlen((char*)wbuf) && buf_cmp(rbuf, wbuf, (uint16_t)br)){
            my_printf(DEBUG_USART, "readback OK: %s\r\n", rbuf);
        } else {
            my_printf(DEBUG_USART, "readback FAIL\r\n");
        }
    }

    lfn_test();
}

void sd_lfs_init(void) { sd_fatfs_init(); }
void sd_lfs_test(void) { sd_fatfs_test(); }

void __aeabi_assert(const char *expr, const char *file, int line)
{
    my_printf(DEBUG_USART, "ASSERT: %s @ %s:%d\r\n",
              expr ? expr : "?", file ? file : "?", line);
    for(;;);
}
