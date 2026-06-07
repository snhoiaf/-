/*
 * BootLoader核心
 * 2026-05 重写
 *
 * 流程:
 *   1. 读参数页双副本，选有效的那个
 *   2. 如果有PENDING标志，做升级: 校验app2 → 拷贝到app1 → 校验app1
 *   3. 正常启动: 校验app1向量表 → 跳转
 *   4. 都不行: 死循环，等debug
 */
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "gd32f4xx.h"
#include "bl_core.h"
#include "bl_config.h"
#include "bl_param.h"
#include "mcu_cmic_gd32f470vet6.h"
#include "usart_app.h"
#include "systick.h"

typedef void (*jump_fn)(void);

static uint8_t page_buf[BL_PARAM_PAGE_SIZE];

#define BOOT_REQ_MAGIC  0xA55A5AA5UL

/* ---- crc32 ---- */

static uint32_t bl_crc(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xFFFFFFFFUL, i, j;
    for(i = 0; i < n; i++){
        c ^= p[i];
        for(j = 0; j < 8; j++){
            if(c & 1) c = (c >> 1) ^ 0xEDB88320UL;
            else      c >>= 1;
        }
    }
    return c ^ 0xFFFFFFFFUL;
}

static uint32_t pcrc(const bl_param_t *p)
{
    return bl_crc((const uint8_t*)p, (uint32_t)offsetof(bl_param_t, param_crc32));
}

/* ---- flash ---- */

static int fmc_busy(void)
{
    uint32_t t = 0x3FFFFFUL;
    while(RESET != fmc_flag_get(FMC_FLAG_BUSY) && t) t--;
    return t ? 1 : 0;
}

static void fmc_clr(void)
{
    fmc_flag_clear(FMC_FLAG_END);
    fmc_flag_clear(FMC_FLAG_WPERR);
    fmc_flag_clear(FMC_FLAG_PGSERR);
    fmc_flag_clear(FMC_FLAG_PGMERR);
}

static int fl_erase(uint32_t addr, uint32_t sz)
{
    uint32_t end, pg;
    if(!sz || addr < BL_FLASH_BASE_ADDR) return 0;
    end = addr + sz - 1;
    if(end > BL_FLASH_END_ADDR) return 0;
    pg  = addr - (addr % BL_FLASH_PAGE_SIZE);
    end = end - (end % BL_FLASH_PAGE_SIZE);
    fmc_unlock();
    fmc_clr();
    while(pg <= end){
        fmc_page_erase(pg);
        if(!fmc_busy()){ fmc_lock(); return 0; }
        fmc_clr();
        pg += BL_FLASH_PAGE_SIZE;
    }
    fmc_lock();
    return 1;
}

static int fl_write(uint32_t addr, const uint8_t *d, uint32_t n)
{
    uint32_t w;
    if(!d || !n) return 0;
    if(addr < BL_FLASH_BASE_ADDR || addr + n - 1 > BL_FLASH_END_ADDR) return 0;
    fmc_unlock();
    fmc_clr();
    while(!(addr & 3) && n >= 4){
        memcpy(&w, d, 4);
        fmc_word_program(addr, w);
        if(!fmc_busy()){ fmc_lock(); return 0; }
        addr += 4; d += 4; n -= 4;
    }
    while(n--){
        fmc_byte_program(addr, *d);
        if(!fmc_busy()){ fmc_lock(); return 0; }
        addr++; d++;
    }
    fmc_clr();
    fmc_lock();
    return 1;
}

/* ---- 参数页 ---- */

static int p_valid(const bl_param_t *p)
{
    if(p->magic != BL_PARAM_MAGIC) return 0;
    if(p->tail_magic != BL_PARAM_TAIL_MAGIC) return 0;
    if(p->version != BL_PARAM_VERSION) return 0;
    if(p->app1_addr != BL_APP1_START_ADDR || p->app2_addr != BL_APP2_START_ADDR) return 0;
    if(p->app_size > BL_APP1_SIZE || p->app_size > BL_APP2_SIZE) return 0;
    return pcrc(p) == p->param_crc32;
}

static void p_default(bl_param_t *p)
{
    memset(p, 0, sizeof(*p));
    p->magic       = BL_PARAM_MAGIC;
    p->version     = BL_PARAM_VERSION;
    p->update_flag = BL_UPDATE_FLAG_IDLE;
    p->app1_addr   = BL_APP1_START_ADDR;
    p->app2_addr   = BL_APP2_START_ADDR;
    p->tail_magic  = BL_PARAM_TAIL_MAGIC;
    p->param_crc32 = pcrc(p);
}

static void p_commit(bl_param_t *p, const bl_log_entry_t *log, int add_log)
{
    uint32_t idx, off;
    bl_param_t m, b;

    memcpy(page_buf, (void*)BL_PARAM_PAGE_ADDR, BL_PARAM_PAGE_SIZE);
    memcpy(&m, p, sizeof(m));
    m.param_crc32 = pcrc(&m);
    b = m;

    memcpy(&page_buf[BL_PARAM_MAIN_ADDR   - BL_PARAM_PAGE_ADDR], &m, sizeof(m));
    memcpy(&page_buf[BL_PARAM_BACKUP_ADDR - BL_PARAM_PAGE_ADDR], &b, sizeof(b));

    if(add_log && log){
        idx = m.log_write_index % BL_LOG_ENTRY_COUNT;
        off = (BL_LOG_ADDR - BL_PARAM_PAGE_ADDR) + idx * BL_LOG_ENTRY_SIZE;
        memcpy(&page_buf[off], log, sizeof(*log));
    }

    fl_erase(BL_PARAM_PAGE_ADDR, BL_PARAM_PAGE_SIZE);
    fl_write(BL_PARAM_PAGE_ADDR, page_buf, BL_PARAM_PAGE_SIZE);
}

bool bl_commit_param(bl_param_t *p)
{
    bl_param_t tmp;
    if(!p_valid(p)){
        p_default(&tmp);
        tmp.update_flag = p->update_flag;
        tmp.app_size    = p->app_size;
        tmp.app_crc32   = p->app_crc32;
        tmp.last_error  = p->last_error;
        *p = tmp;
    }
    p->magic      = BL_PARAM_MAGIC;
    p->version    = BL_PARAM_VERSION;
    p->app1_addr  = BL_APP1_START_ADDR;
    p->app2_addr  = BL_APP2_START_ADDR;
    p->tail_magic = BL_PARAM_TAIL_MAGIC;
    p_commit(p, NULL, 0);
    return true;
}

/* ---- 日志 ---- */

static void mklog(bl_log_entry_t *e, uint32_t seq, uint32_t evt,
                   uint32_t res, uint32_t v0, uint32_t v1, uint32_t v2)
{
    memset(e, 0, sizeof(*e));
    e->magic    = BL_LOG_MAGIC;
    e->seq      = seq;
    e->event_id = evt;
    e->result   = res;
    e->value0   = v0;
    e->value1   = v1;
    e->value2   = v2;
    e->crc32    = bl_crc((const uint8_t*)e, (uint32_t)offsetof(bl_log_entry_t, crc32));
}

void bl_log_dump_uart(void)
{
    uint32_t i;
    const bl_log_entry_t *e;
    uint32_t cnt = 0;

    my_printf(DEBUG_USART, "BL log:\r\n");
    for(i = 0; i < BL_LOG_ENTRY_COUNT; i++){
        e = (const bl_log_entry_t*)(BL_LOG_ADDR + i * BL_LOG_ENTRY_SIZE);
        if(e->magic != BL_LOG_MAGIC) continue;
        my_printf(DEBUG_USART, "  [%02u] seq=%u evt=%u res=%u v0=0x%08X v1=0x%08X v2=0x%08X crc=%s\r\n",
                  i, e->seq, e->event_id, e->result, e->value0, e->value1, e->value2,
                  (bl_crc((const uint8_t*)e, offsetof(bl_log_entry_t, crc32)) == e->crc32) ? "OK" : "BAD");
        cnt++;
    }
    if(!cnt) my_printf(DEBUG_USART, "  <empty>\r\n");
}

/* ---- app校验和跳转 ---- */

static int app_vec_ok(uint32_t base)
{
    uint32_t msp = *(volatile uint32_t*)base;
    uint32_t rst = *(volatile uint32_t*)(base + 4);
    if((msp & 0x2FFE0000UL) != 0x20000000UL) return 0;
    if(rst < BL_FLASH_BASE_ADDR || rst > BL_FLASH_END_ADDR) return 0;
    return 1;
}

static int app2_copy(uint32_t sz)
{
    uint8_t tmp[BL_COPY_CHUNK_SIZE];
    uint32_t done = 0, chunk, left;
    uint32_t erase_sz;

    if(!sz || sz > BL_APP1_SIZE || sz > BL_APP2_SIZE) return 0;
    erase_sz = (sz + BL_FLASH_PAGE_SIZE - 1) & ~(BL_FLASH_PAGE_SIZE - 1);
    if(!fl_erase(BL_APP1_START_ADDR, erase_sz)) return 0;

    while(done < sz){
        left = sz - done;
        chunk = (left > BL_COPY_CHUNK_SIZE) ? BL_COPY_CHUNK_SIZE : left;
        memcpy(tmp, (void*)(BL_APP2_START_ADDR + done), chunk);
        if(!fl_write(BL_APP1_START_ADDR + done, tmp, chunk)) return 0;
        done += chunk;
    }
    return 1;
}

static void do_jump(uint32_t base)
{
    uint32_t rst, i;
    __disable_irq();
    SysTick->CTRL = 0; SysTick->LOAD = 0; SysTick->VAL = 0;
    for(i = 0; i < 8; i++){
        NVIC->ICER[i] = 0xFFFFFFFFUL;
        NVIC->ICPR[i] = 0xFFFFFFFFUL;
    }
    __DSB(); __ISB();
    SCB->VTOR = base;
    __set_MSP(*(volatile uint32_t*)base);
    rst = *(volatile uint32_t*)(base + 4);
    __enable_irq();
    ((jump_fn)rst)();
}

static uint8_t boot_request_take(void)
{
    uint8_t requested;

    rcu_periph_clock_enable(RCU_PMU);
    pmu_backup_write_enable();

    requested = (RTC_BKP1 == BOOT_REQ_MAGIC) ? 1U : 0U;
    if(requested){
        RTC_BKP1 = 0U;
    }
    return requested;
}

/* ---- 协议帧相关 ---- */

/* 外部变量：来自gd32f4xx_it.c */
extern volatile uint32_t usart1_rx_len;
extern volatile uint8_t  usart1_rx_flag;
extern uint8_t usart1_rx_buf[USART1_RX_BUF_SIZE];

#define PROTO_HEAD_HI   0xA5U
#define PROTO_HEAD_LO   0xB6U
#define PROTO_TAIL_HI   0xB6U
#define PROTO_TAIL_LO   0xA5U
#define PROTO_TYPE_RSP  0x02U
#define PROTO_TYPE_ERR  0xFFU
#define PROTO_CMD_0502  0x0502U
#define PROTO_CMD_0503  0x0503U
#define PROTO_CMD_EEEE  0xEEEEU
#define FIRMWARE_MAGIC  0x5AA5C33CUL
#define OTA_CHUNK_SIZE  256U

static uint16_t bl_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    uint16_t i;
    uint8_t  bit;
    for(i = 0; i < len; i++){
        crc ^= data[i];
        for(bit = 0; bit < 8U; bit++){
            if(crc & 0x0001U) crc = (uint16_t)((crc >> 1) ^ 0xA001U);
            else crc >>= 1;
        }
    }
    return crc;
}

/* ASCII HEX字符串 → 二进制，返回转换字节数 */
static uint32_t ascii_hex_to_bin(const uint8_t *src, uint32_t src_len, uint8_t *dst, uint32_t dst_max)
{
    uint32_t i, out = 0;
    uint8_t hi, lo, h, l;
    for(i = 0; i + 1 < src_len && out < dst_max; i += 2){
        h = src[i];     lo = src[i+1];
        hi = (h  >= 'A') ? (h  - 'A' + 10U) : (h  >= 'a') ? (h  - 'a' + 10U) : (h  - '0');
        l  = (lo >= 'A') ? (lo - 'A' + 10U) : (lo >= 'a') ? (lo - 'a' + 10U) : (lo - '0');
        dst[out++] = (uint8_t)((hi << 4) | l);
    }
    return out;
}

/* 发送RS485帧（ASCII HEX格式） */
static void bl_send_frame(uint16_t dev_id, uint8_t type, uint16_t cmd,
                           const uint8_t *payload, uint8_t plen)
{
    uint8_t  raw[16];
    uint32_t pos = 0;
    uint16_t crc;
    char     out[64];
    int      n, i;

    raw[pos++] = PROTO_HEAD_HI;
    raw[pos++] = PROTO_HEAD_LO;
    raw[pos++] = (uint8_t)(dev_id >> 8);
    raw[pos++] = (uint8_t)(dev_id);
    raw[pos++] = type;
    raw[pos++] = (uint8_t)(cmd >> 8);
    raw[pos++] = (uint8_t)(cmd);
    raw[pos++] = plen;
    raw[pos++] = 0x02U;
    for(i = 0; i < plen && pos < sizeof(raw); i++) raw[pos++] = payload[i];

    crc = bl_crc16(raw, (uint16_t)pos);

    n = snprintf(out, sizeof(out), "A5B6%04X%02X%04X%02X02",
                 dev_id, type, cmd, plen);
    for(i = 0; i < plen; i++) n += snprintf(out + n, sizeof(out) - (size_t)n, "%02X", payload[i]);
    n += snprintf(out + n, sizeof(out) - (size_t)n, "%04X", crc);
    n += snprintf(out + n, sizeof(out) - (size_t)n, "B6A5");

    gpio_bit_set(USART1_DIR_PORT, USART1_DIR_PIN);
    usart_interrupt_disable(USART1, USART_INT_IDLE);
    delay_1ms(1);
    for(i = 0; i < n; i++){
        usart_data_transmit(USART1, (uint8_t)out[i]);
        while(RESET == usart_flag_get(USART1, USART_FLAG_TBE));
    }
    while(RESET == usart_flag_get(USART1, USART_FLAG_TC));
    delay_1ms(1);
    gpio_bit_reset(USART1_DIR_PORT, USART1_DIR_PIN);
    usart_data_receive(USART1);
    usart_interrupt_flag_clear(USART1, USART_INT_FLAG_IDLE);
    usart_interrupt_enable(USART1, USART_INT_IDLE);
}

static void bl_send_ok(uint16_t dev_id, uint16_t cmd)
{
    uint8_t rsp = 0xFFU;
    bl_send_frame(dev_id, PROTO_TYPE_RSP, cmd, &rsp, 1U);
}

static void bl_send_err(uint16_t dev_id)
{
    bl_send_frame(dev_id, PROTO_TYPE_ERR, PROTO_CMD_EEEE, NULL, 0U);
}

/*
 * 从ASCII HEX帧缓冲中解析命令字，返回cmd；失败返回0
 * 格式: A5B6 XXXX XX XXXX XX XX [payload] XXXX B6A5
 * 按字节位置: [0-3]head [4-7]id [8-9]type [10-13]cmd [14-15]len [16-17]ver ...
 */
static uint16_t bl_parse_cmd(const uint8_t *buf, uint32_t len, uint16_t *out_dev_id)
{
    uint8_t  raw[32];
    uint32_t raw_len, total_chars;
    uint16_t cmd, dev_id, crc_rx, crc_calc;
    uint8_t  payload_len;

    /* 去掉末尾\r\n等空白 */
    while(len > 0U && (buf[len-1U] == '\r' || buf[len-1U] == '\n' || buf[len-1U] == 0x00U)){
        len--;
    }

    /* 最小帧22字符 */
    if(len < 22U) return 0U;
    if(buf[0]!='A'||buf[1]!='5'||buf[2]!='B'||buf[3]!='6') return 0U;
    if(buf[len-4]!='B'||buf[len-3]!='6'||buf[len-2]!='A'||buf[len-1]!='5') return 0U;

    raw_len = ascii_hex_to_bin(buf, len - 4U, raw, sizeof(raw));
    if(raw_len < 9U) return 0U;

    dev_id      = (uint16_t)((raw[2] << 8) | raw[3]);
    payload_len =  raw[7];
    total_chars = 4U + (uint32_t)(9U + payload_len + 2U) * 2U + 4U;
    if(len < total_chars) return 0U;

    crc_rx   = (uint16_t)((raw[raw_len - 2U] << 8) | raw[raw_len - 1U]);
    crc_calc = bl_crc16(raw, (uint16_t)(raw_len - 2U));
    if(crc_rx != crc_calc) return 0U;

    cmd = (uint16_t)((raw[5] << 8) | raw[6]);
    if(out_dev_id) *out_dev_id = dev_id;
    return cmd;
}

/*
 * 等待并接收OTA固件包（裸bin，256字节切片，5ms间隔）
 * 写入固件暂存区BL_APP2_START_ADDR，校验magic后提交参数页
 * 返回实际接收字节数，失败返回0
 */
static uint32_t bl_recv_firmware(uint16_t dev_id, bl_param_t *work)
{
    uint32_t total = 0U;
    uint32_t chunk_len;
    uint32_t erase_done = 0U;
    uint32_t timeout;

    /* 擦除固件暂存区 */
    if(!fl_erase(BL_APP2_START_ADDR, BL_APP2_SIZE)) return 0U;
    erase_done = 1U;
    (void)erase_done;

    /* 按切片接收，每片最多256字节，超时50ms无数据则认为结束 */
    while(total < BL_APP2_SIZE){
        timeout = 200U; /* 200×1ms = 200ms超时 */
        while(!usart1_rx_flag && timeout) { delay_1ms(1); timeout--; }
        if(!usart1_rx_flag) break;

        chunk_len = usart1_rx_len;
        if(total + chunk_len > BL_APP2_SIZE) chunk_len = BL_APP2_SIZE - total;

        if(!fl_write(BL_APP2_START_ADDR + total, usart1_rx_buf, chunk_len)){
            return 0U;
        }
        total += chunk_len;
        usart1_rx_flag = 0U;
        usart1_rx_len  = 0U;

        /* 重置DMA继续接收 */
        dma_channel_disable(USART1_RX_DMA_PERIPH, USART1_RX_DMA_CHANNEL);
        dma_flag_clear(USART1_RX_DMA_PERIPH, USART1_RX_DMA_CHANNEL, DMA_FLAG_FTF);
        dma_transfer_number_config(USART1_RX_DMA_PERIPH, USART1_RX_DMA_CHANNEL, USART1_RX_BUF_SIZE);
        dma_channel_enable(USART1_RX_DMA_PERIPH, USART1_RX_DMA_CHANNEL);
    }

    if(total < 4U) return 0U;

    /* 校验固件magic（前4字节） */
    uint32_t magic;
    memcpy(&magic, (void*)BL_APP2_START_ADDR, 4U);
    if(magic != FIRMWARE_MAGIC) return 0U;

    /* 剥离magic，实际App从第5字节开始，大小=total-4 */
    uint32_t app_size = total - 4U;
    if(app_size == 0U || app_size > BL_APP2_SIZE) return 0U;

    /* 把App内容向前移4字节（擦除后重写） */
    static uint8_t tmp_page[BL_FLASH_PAGE_SIZE];
    uint32_t done = 0U, chunk;
    fl_erase(BL_APP2_START_ADDR, (app_size + BL_FLASH_PAGE_SIZE - 1U) & ~(BL_FLASH_PAGE_SIZE - 1U));
    while(done < app_size){
        chunk = app_size - done;
        if(chunk > sizeof(tmp_page)) chunk = sizeof(tmp_page);
        /* 从Flash+4偏移读（已写入的原始数据） */
        memcpy(tmp_page, (void*)(BL_APP2_START_ADDR + 4U + done), chunk);
        fl_write(BL_APP2_START_ADDR + done, tmp_page, chunk);
        done += chunk;
    }

    /* 提交参数页PENDING */
    work->update_flag = BL_UPDATE_FLAG_PENDING;
    work->app_size    = app_size;
    work->app_crc32   = bl_crc((const uint8_t*)BL_APP2_START_ADDR, app_size);
    work->app1_addr   = BL_APP1_START_ADDR;
    work->app2_addr   = BL_APP2_START_ADDR;

    return app_size;
}

/* BL专用：发字符串，切RS485方向，发完立即释放，发送期间禁用空闲中断防回声 */
static void bl_puts(const char *s)
{
    usart_interrupt_disable(USART1, USART_INT_IDLE);
    gpio_bit_set(USART1_DIR_PORT, USART1_DIR_PIN);
    while(*s){
        usart_data_transmit(USART1, (uint8_t)*s);
        while(RESET == usart_flag_get(USART1, USART_FLAG_TBE));
        s++;
    }
    while(RESET == usart_flag_get(USART1, USART_FLAG_TC));
    gpio_bit_reset(USART1_DIR_PORT, USART1_DIR_PIN);
    /* 清除IDLE标志，重置DMA，清空buf和flag */
    usart_data_receive(USART1);
    usart_interrupt_flag_clear(USART1, USART_INT_FLAG_IDLE);
    dma_channel_disable(USART1_RX_DMA_PERIPH, USART1_RX_DMA_CHANNEL);
    dma_flag_clear(USART1_RX_DMA_PERIPH, USART1_RX_DMA_CHANNEL, DMA_FLAG_FTF);
    memset(usart1_rx_buf, 0, USART1_RX_BUF_SIZE);
    dma_transfer_number_config(USART1_RX_DMA_PERIPH, USART1_RX_DMA_CHANNEL, USART1_RX_BUF_SIZE);
    dma_channel_enable(USART1_RX_DMA_PERIPH, USART1_RX_DMA_CHANNEL);
    usart1_rx_flag = 0U;
    usart1_rx_len  = 0U;
    usart_interrupt_enable(USART1, USART_INT_IDLE);
}

static void bootloader_wait_cmd(bl_param_t *work)
{
    uint16_t cmd, dev_id = 0x0001U;
    uint32_t app_size;
    uint8_t  rsp;
    uint32_t tick;
    uint32_t total_ticks = 1000U; /* 10s × 100次/s */
    uint32_t last_sec = 0xFFFFU;
    uint32_t cur_sec;

    bl_puts("using command to interrupt start Application\r\n");
    bl_send_frame(dev_id, 0x05U, 0x8888U, NULL, 0U);

    for(tick = 0; tick < total_ticks; tick++){
        cur_sec = 10U - tick / 100U;
        if(cur_sec != last_sec){
            last_sec = cur_sec;
            char tmp[64];
            snprintf(tmp, sizeof(tmp), "wait for start Application(%us)......\r\n", (unsigned)cur_sec);
            bl_puts(tmp);
            /* bl_puts内部已清空flag和DMA */
        }

        delay_1ms(10);

        if(usart1_rx_flag){
            cmd = bl_parse_cmd(usart1_rx_buf, usart1_rx_len, &dev_id);
            usart1_rx_flag = 0U;
            usart1_rx_len  = 0U;

            if(cmd == PROTO_CMD_0502){
                bl_send_ok(dev_id, PROTO_CMD_0502);
                app_size = bl_recv_firmware(dev_id, work);

                if(app_size > 0U){
                    rsp = 0xFFU;
                    bl_send_frame(dev_id, PROTO_TYPE_RSP, PROTO_CMD_0502, &rsp, 1U);

                    uint32_t t = 5000U;
                    while(t--){
                        delay_1ms(1);
                        if(usart1_rx_flag){
                            cmd = bl_parse_cmd(usart1_rx_buf, usart1_rx_len, NULL);
                            usart1_rx_flag = 0U;
                            usart1_rx_len  = 0U;
                            if(cmd == PROTO_CMD_0503){
                                bl_send_ok(dev_id, PROTO_CMD_0503);
                                delay_1ms(50);
                                p_commit(work, NULL, 0);
                                NVIC_SystemReset();
                            }
                        }
                    }
                } else {
                    bl_send_err(dev_id);
                }
                return;
            }
        }
    }
}

/* ---- 主流程 ---- */

void bootloader_run(void)
{
    bl_param_t main_p, back_p, work;
    bl_log_entry_t log;
    int main_ok, back_ok, need_fix = 0;
    uint32_t crc, seq;
    uint8_t wait_upgrade;

    wait_upgrade = boot_request_take();
    memcpy(&main_p, (void*)BL_PARAM_MAIN_ADDR, sizeof(main_p));
    memcpy(&back_p, (void*)BL_PARAM_BACKUP_ADDR, sizeof(back_p));
    main_ok = p_valid(&main_p);
    back_ok = p_valid(&back_p);

    /* 选有效的副本，都有效取update_counter大的 */
    if(main_ok && back_ok){
        work = (main_p.update_counter >= back_p.update_counter) ? main_p : back_p;
        if(&work != &main_p) need_fix = 1;
    } else if(main_ok){
        work = main_p; need_fix = 1;
    } else if(back_ok){
        work = back_p; need_fix = 1;
    } else {
        p_default(&work);
        work.last_error = BL_ERR_PARAM_INVALID;
        need_fix = 1;
    }

    /* 修复参数页 */
    if(need_fix){
        seq = work.update_counter + work.fail_counter;
        mklog(&log, seq, BL_LOG_EVENT_PARAM_RECOVER, 1,
               main_ok ? 1 : 0, back_ok ? 1 : 0, work.last_error);
        work.log_write_index = (work.log_write_index + 1) % BL_LOG_ENTRY_COUNT;
        p_commit(&work, &log, 1);
    }

    /* 升级路径 */
    if(work.update_flag == BL_UPDATE_FLAG_PENDING){
        /* 校验app2向量表 */
        if(!work.app_size || work.app_size > BL_APP2_SIZE || !app_vec_ok(BL_APP2_START_ADDR)){
            work.update_flag = BL_UPDATE_FLAG_FAILED;
            work.fail_counter++;
            work.last_error = BL_ERR_APP2_INVALID;
            seq = work.update_counter + work.fail_counter;
            mklog(&log, seq, BL_LOG_EVENT_UPDATE_FAIL, 0, BL_ERR_APP2_INVALID, work.app_size, 0);
            work.log_write_index = (work.log_write_index + 1) % BL_LOG_ENTRY_COUNT;
            p_commit(&work, &log, 1);
            NVIC_SystemReset();
        }

        /* 校验app2内容 */
        crc = bl_crc((const uint8_t*)BL_APP2_START_ADDR, work.app_size);
        if(crc != work.app_crc32){
            work.update_flag = BL_UPDATE_FLAG_FAILED;
            work.fail_counter++;
            work.last_error = BL_ERR_APP2_INVALID;
            seq = work.update_counter + work.fail_counter;
            mklog(&log, seq, BL_LOG_EVENT_UPDATE_FAIL, 0, BL_ERR_APP2_INVALID, work.app_crc32, crc);
            work.log_write_index = (work.log_write_index + 1) % BL_LOG_ENTRY_COUNT;
            p_commit(&work, &log, 1);
            NVIC_SystemReset();
        }

        /* 拷贝app2→app1 */
        if(app2_copy(work.app_size)){
            /* 校验app1 */
            crc = bl_crc((const uint8_t*)BL_APP1_START_ADDR, work.app_size);
            if(crc == work.app_crc32){
                work.update_flag = BL_UPDATE_FLAG_IDLE;
                work.update_counter++;
                work.last_error = BL_ERR_NONE;
                seq = work.update_counter + work.fail_counter;
                mklog(&log, seq, BL_LOG_EVENT_UPDATE_OK, 1, work.app_size, work.app_crc32, crc);
            } else {
                work.update_flag = BL_UPDATE_FLAG_FAILED;
                work.fail_counter++;
                work.last_error = BL_ERR_COPY_FAILED;
                seq = work.update_counter + work.fail_counter;
                mklog(&log, seq, BL_LOG_EVENT_UPDATE_FAIL, 0, BL_ERR_COPY_FAILED, work.app_crc32, crc);
            }
        } else {
            work.update_flag = BL_UPDATE_FLAG_FAILED;
            work.fail_counter++;
            work.last_error = BL_ERR_COPY_FAILED;
            seq = work.update_counter + work.fail_counter;
            mklog(&log, seq, BL_LOG_EVENT_UPDATE_FAIL, 0, BL_ERR_COPY_FAILED, 0, 0);
        }

        work.log_write_index = (work.log_write_index + 1) % BL_LOG_ENTRY_COUNT;
        p_commit(&work, &log, 1);
        NVIC_SystemReset();
    }

    /* 正常启动 */
    if(app_vec_ok(BL_APP1_START_ADDR)){
        if(wait_upgrade){
            bootloader_wait_cmd(&work);
        } else {
            delay_1ms(5000);
        }
        do_jump(BL_APP1_START_ADDR);
    }

    /* 没有可用app */
    bl_log_dump_uart();
    work.last_error = BL_ERR_APP1_INVALID;
    seq = work.update_counter + work.fail_counter;
    mklog(&log, seq, BL_LOG_EVENT_JUMP_FAIL, 0, BL_ERR_APP1_INVALID, BL_APP1_START_ADDR, 0);
    work.log_write_index = (work.log_write_index + 1) % BL_LOG_ENTRY_COUNT;
    p_commit(&work, &log, 1);

    for(;;);
}
