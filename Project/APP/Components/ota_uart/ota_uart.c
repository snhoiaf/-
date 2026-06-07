/*
 * OTA升级模块
 * DMA接收 → 环形缓冲 → 解帧 → 写Flash → 通知BootLoader
 */
#include "mcu_cmic_gd32f470vet6.h"
#include "usart_app.h"
#include "ota_uart.h"
#include "ring_buffer.h"
#include "bl_partition.h"
#include "bl_param.h"
#include "ota_protocol.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define RB_SIZE         (16 * 1024)
#define PAUSE_LEVEL     (RB_SIZE * 80 / 100)
#define RESUME_LEVEL    (RB_SIZE * 20 / 100)

enum { STATE_IDLE = 0, STATE_DATA = 1 };

typedef struct {
    uint8_t     state;
    uint32_t    fw_ver;
    uint32_t    fw_size;
    uint32_t    fw_crc;
    uint32_t    rx_bytes;
    uint32_t    crc_val;
    uint16_t    seq_num;
    uint8_t     is_paused;
} ota_info_t;

static ota_info_t info;
static uint8_t    tmp_buf[BL_PARAM_PAGE_SIZE];
static uint8_t    rb_mem[RB_SIZE];
static ring_buffer_t rb;

static uint32_t crc_calc(const uint8_t *buf, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    uint32_t i, b;
    for(i = 0; i < len; i++){
        crc ^= buf[i];
        for(b = 0; b < 8; b++){
            if(crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
            else        crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFF;
}

static uint32_t crc_run(uint32_t crc, const uint8_t *buf, uint32_t len)
{
    uint32_t i, b;
    for(i = 0; i < len; i++){
        crc ^= buf[i];
        for(b = 0; b < 8; b++){
            if(crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
            else        crc >>= 1;
        }
    }
    return crc;
}

static void uart_put(const uint8_t *d, uint32_t n)
{
    uint32_t i;
    for(i = 0; i < n; i++){
        usart_data_transmit(OTA_UART_PERIPH, d[i]);
        while(RESET == usart_flag_get(OTA_UART_PERIPH, USART_FLAG_TBE));
    }
    while(RESET == usart_flag_get(OTA_UART_PERIPH, USART_FLAG_TC));
}

static void uart_str(const char *s)
{
    uart_put((const uint8_t*)s, (uint32_t)strlen(s));
}

static void send_frame(uint8_t type, uint8_t status, uint16_t seq, uint32_t offset)
{
    ota_frm_t f;
    memset(&f, 0, sizeof(f));
    f.sync = OTA_SYNC;
    f.ft   = type;
    f.st   = status;
    f.seq  = seq;
    f.off  = offset;
    uart_put((const uint8_t*)&f, sizeof(f));
}

static void send_ack(uint16_t seq, uint32_t off)
{
    send_frame(OTA_FT_ACK, 0, seq, off);
}

static void send_nack(uint16_t seq, uint32_t off, uint8_t code)
{
    send_frame(OTA_FT_NACK, code, seq, off);
}

__attribute__((section(".ramfunc"), noinline))
static int wait_fmc(void)
{
    uint32_t timeout = 0x3FFFFF;
    while(RESET != fmc_flag_get(FMC_FLAG_BUSY) && timeout) timeout--;
    return timeout ? 1 : 0;
}

__attribute__((section(".ramfunc"), noinline))
static void clear_fmc(void)
{
    fmc_flag_clear(FMC_FLAG_END);
    fmc_flag_clear(FMC_FLAG_WPERR);
    fmc_flag_clear(FMC_FLAG_PGSERR);
    fmc_flag_clear(FMC_FLAG_PGMERR);
}

__attribute__((section(".ramfunc"), noinline))
static int erase_flash(uint32_t addr, uint32_t size)
{
    uint32_t end_addr, page;

    if(!size || addr < BL_FLASH_BASE_ADDR) return 0;
    end_addr = addr + size - 1;
    if(end_addr > BL_FLASH_END_ADDR) return 0;

    page = addr - (addr % BL_FLASH_PAGE_SIZE);
    end_addr = end_addr - (end_addr % BL_FLASH_PAGE_SIZE);

    fmc_unlock();
    clear_fmc();
    while(page <= end_addr){
        fmc_page_erase(page);
        if(!wait_fmc()){ fmc_lock(); return 0; }
        clear_fmc();
        page += BL_FLASH_PAGE_SIZE;
    }
    fmc_lock();
    return 1;
}

__attribute__((section(".ramfunc"), noinline))
static int write_flash(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t word;

    if(!data || !len) return 0;
    if(addr < BL_FLASH_BASE_ADDR || addr + len - 1 > BL_FLASH_END_ADDR) return 0;

    fmc_unlock();
    clear_fmc();

    while(!(addr & 3) && len >= 4){
        memcpy(&word, data, 4);
        fmc_word_program(addr, word);
        if(!wait_fmc()){ fmc_lock(); return 0; }
        addr += 4; data += 4; len -= 4;
    }
    while(len--){
        fmc_byte_program(addr, *data);
        if(!wait_fmc()){ fmc_lock(); return 0; }
        addr++; data++;
    }

    clear_fmc();
    fmc_lock();
    return 1;
}

static uint32_t get_param_crc(const bl_param_t *p)
{
    return crc_calc((const uint8_t*)p, (uint32_t)offsetof(bl_param_t, param_crc32));
}

static int check_param(const bl_param_t *p)
{
    return p->magic == BL_PARAM_MAGIC &&
           p->tail_magic == BL_PARAM_TAIL_MAGIC &&
           p->version == BL_PARAM_VERSION;
}

static void init_param(bl_param_t *p)
{
    memset(p, 0, sizeof(*p));
    p->magic       = BL_PARAM_MAGIC;
    p->version     = BL_PARAM_VERSION;
    p->update_flag = BL_UPDATE_FLAG_IDLE;
    p->app1_addr   = BL_APP1_START_ADDR;
    p->app2_addr   = BL_APP2_START_ADDR;
    p->tail_magic  = BL_PARAM_TAIL_MAGIC;
    p->param_crc32 = get_param_crc(p);
}

static int save_param(uint32_t size, uint32_t crc)
{
    bl_param_t main_p, backup_p;

    memcpy(tmp_buf, (void*)BL_PARAM_PAGE_ADDR, BL_PARAM_PAGE_SIZE);
    memcpy(&main_p, (void*)BL_PARAM_MAIN_ADDR, sizeof(main_p));
    memcpy(&backup_p, (void*)BL_PARAM_BACKUP_ADDR, sizeof(backup_p));

    if(!check_param(&main_p)){
        if(check_param(&backup_p)) main_p = backup_p;
        else init_param(&main_p);
    }

    main_p.app_size    = size;
    main_p.app_crc32   = crc;
    main_p.app1_addr   = BL_APP1_START_ADDR;
    main_p.app2_addr   = BL_APP2_START_ADDR;
    main_p.update_flag = BL_UPDATE_FLAG_PENDING;
    main_p.last_error  = BL_ERR_NONE;
    main_p.param_crc32 = get_param_crc(&main_p);
    backup_p = main_p;

    memcpy(&tmp_buf[BL_PARAM_MAIN_ADDR   - BL_PARAM_PAGE_ADDR], &main_p, sizeof(main_p));
    memcpy(&tmp_buf[BL_PARAM_BACKUP_ADDR - BL_PARAM_PAGE_ADDR], &backup_p, sizeof(backup_p));

    if(!erase_flash(BL_PARAM_PAGE_ADDR, BL_PARAM_PAGE_SIZE)) return 0;
    return write_flash(BL_PARAM_PAGE_ADDR, tmp_buf, BL_PARAM_PAGE_SIZE);
}

static void check_flow(void)
{
    uint32_t used = ring_buffer_available(&rb);

    if(!info.is_paused && used >= PAUSE_LEVEL){
        info.is_paused = 1;
        uart_str("PAUSE\r\n");
    } else if(info.is_paused && used <= RESUME_LEVEL){
        info.is_paused = 0;
        uart_str("RESUME\r\n");
    }
}

void ota_reset(void)
{
    memset(&info, 0, sizeof(info));
    info.state    = STATE_IDLE;
    info.crc_val  = 0xFFFFFFFF;
    info.seq_num  = 0;
    info.is_paused = 0;
    ring_buffer_init(&rb, rb_mem, RB_SIZE);
}

static int do_start(const ota_meta_t *meta)
{
    uint32_t erase_sz;

    if(!meta->size || meta->size > BL_APP2_SIZE || meta->sync != OTA_SYNC)
        return 0;

    erase_sz = (meta->size + BL_FLASH_PAGE_SIZE - 1) & ~(BL_FLASH_PAGE_SIZE - 1);
    if(!erase_flash(BL_APP2_START_ADDR, erase_sz)) return 0;

    info.state    = STATE_DATA;
    info.fw_ver   = meta->ver;
    info.fw_size  = meta->size;
    info.fw_crc   = meta->crc;
    info.rx_bytes = 0;
    info.crc_val  = 0xFFFFFFFF;
    info.seq_num  = 1;

    my_printf(DEBUG_USART, "OTA: start ver=0x%08X sz=%u crc=0x%08X\r\n",
              info.fw_ver, info.fw_size, info.fw_crc);
    return 1;
}

static int do_write(const uint8_t *data, uint32_t len)
{
    uint32_t addr;

    if(!data || !len) return 1;
    if(info.rx_bytes + len > info.fw_size){
        my_printf(DEBUG_USART, "OTA: overflow!\r\n");
        return 0;
    }

    addr = BL_APP2_START_ADDR + info.rx_bytes;
    if(!write_flash(addr, data, len)){
        my_printf(DEBUG_USART, "OTA: flash err\r\n");
        return 0;
    }

    info.crc_val = crc_run(info.crc_val, data, len);
    info.rx_bytes += len;
    return 1;
}

static int do_finish(void)
{
    uint32_t final_crc;

    if(info.rx_bytes != info.fw_size) return 0;

    final_crc = info.crc_val ^ 0xFFFFFFFF;
    if(final_crc != info.fw_crc){
        my_printf(DEBUG_USART, "OTA: crc mismatch 0x%08X vs 0x%08X\r\n",
                  info.fw_crc, final_crc);
        return 0;
    }
    if(!save_param(info.fw_size, info.fw_crc)){
        my_printf(DEBUG_USART, "OTA: commit fail\r\n");
        return 0;
    }
    return 1;
}

static int parse_header(const uint8_t *data, uint16_t len, ota_meta_t *out)
{
    ota_meta_v2_t v2;
    uint32_t hdr_crc;

    if(len == sizeof(ota_meta_t)){
        memcpy(out, data, sizeof(*out));
        return 1;
    }
    if(len != sizeof(v2)) return 0;

    memcpy(&v2, data, sizeof(v2));
    if(v2.sync != OTA_SYNC || v2.hdr_ver != OTA_HDR_V2_VER) return 0;
    if(v2.hdr_sz != sizeof(v2)) return 0;
    if(v2.dst != BL_APP1_START_ADDR || v2.img_type != OTA_IMG_TYPE_APP) return 0;
    if(!v2.size || v2.size > BL_APP2_SIZE) return 0;

    hdr_crc = crc_calc(data, (uint32_t)offsetof(ota_meta_v2_t, hdr_crc));
    if(hdr_crc != v2.hdr_crc) return 0;

    out->sync = v2.sync;
    out->ver  = v2.ver;
    out->size = v2.size;
    out->crc  = v2.crc;
    return 1;
}

void ota_feed(const uint8_t *data, uint32_t len)
{
    if(!data || !len) return;
    if(!ring_buffer_write(&rb, data, len)){
        my_printf(DEBUG_USART, "OTA: ring full\r\n");
        return;
    }
    check_flow();
}

void ota_poll(void)
{
    ota_frm_t frm;
    ota_meta_t meta;
    uint8_t payload[OTA_PL_MAX];
    uint32_t avail;

    for(;;){
        avail = ring_buffer_available(&rb);
        if(avail < sizeof(ota_frm_t)) return;

        ring_buffer_peek(&rb, (uint8_t*)&frm, sizeof(frm));

        if(frm.sync != OTA_SYNC){
            ring_buffer_drop(&rb, 1);
            continue;
        }

        if(frm.plen > OTA_PL_MAX){
            ring_buffer_drop(&rb, 1);
            send_nack(frm.seq, info.rx_bytes, OTA_NK_LEN);
            continue;
        }

        if(avail < sizeof(ota_frm_t) + frm.plen) return;

        ring_buffer_drop(&rb, sizeof(ota_frm_t));
        if(frm.plen > 0)
            ring_buffer_read(&rb, payload, frm.plen);

        if(frm.plen > 0 && crc_calc(payload, frm.plen) != frm.fcrc){
            send_nack(frm.seq, info.rx_bytes, OTA_NK_CRC);
            continue;
        }

        if(frm.ft == OTA_FT_START){
            if(frm.seq || frm.off || !parse_header(payload, frm.plen, &meta)){
                send_nack(frm.seq, info.rx_bytes, OTA_NK_LEN);
                continue;
            }
            if(info.state == STATE_DATA &&
               meta.sync == OTA_SYNC &&
               meta.ver == info.fw_ver &&
               meta.size == info.fw_size &&
               meta.crc == info.fw_crc){
                send_ack(frm.seq, info.rx_bytes);
                continue;
            }
            if(!do_start(&meta)){
                send_nack(frm.seq, 0, OTA_NK_FLASH);
                ota_reset();
                return;
            }
            send_ack(frm.seq, info.rx_bytes);

        } else if(frm.ft == OTA_FT_DATA){
            if(info.state != STATE_DATA){
                send_nack(frm.seq, info.rx_bytes, OTA_NK_STATE);
                continue;
            }
            if((uint16_t)(info.seq_num - 1) == frm.seq &&
               frm.off + frm.plen <= info.rx_bytes){
                send_ack(frm.seq, info.rx_bytes);
                continue;
            }
            if(frm.seq != info.seq_num){
                send_nack(frm.seq, info.rx_bytes, OTA_NK_SEQ);
                continue;
            }
            if(frm.off != info.rx_bytes){
                send_nack(frm.seq, info.rx_bytes, OTA_NK_OFFSET);
                continue;
            }
            if(!do_write(payload, frm.plen)){
                send_nack(frm.seq, info.rx_bytes, OTA_NK_FLASH);
                ota_reset();
                return;
            }
            info.seq_num++;
            send_ack(frm.seq, info.rx_bytes);

            if(!(info.rx_bytes % 4096) || info.rx_bytes == info.fw_size)
                my_printf(DEBUG_USART, "OTA: %u/%u (%u%%)\r\n",
                          info.rx_bytes, info.fw_size, info.rx_bytes * 100 / info.fw_size);

        } else if(frm.ft == OTA_FT_END){
            if(info.state != STATE_DATA || frm.seq != info.seq_num ||
               frm.off != info.rx_bytes || frm.plen || frm.fcrc != info.fw_crc){
                send_nack(frm.seq, info.rx_bytes, OTA_NK_FINAL);
                continue;
            }
            if(!do_finish()){
                send_nack(frm.seq, info.rx_bytes, OTA_NK_FINAL);
                ota_reset();
                return;
            }
            send_ack(frm.seq, info.rx_bytes);
            my_printf(DEBUG_USART, "OTA: done, rebooting\r\n");
            __disable_irq();
            NVIC_SystemReset();

        } else {
            send_nack(frm.seq, info.rx_bytes, OTA_NK_STATE);
        }

        check_flow();
    }
}
