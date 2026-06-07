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

static void bootloader_wait_cmd(void)
{
    my_printf(USART1, "using command to interrupt start Application\r\n");
    my_printf(USART1, "wait for start Application(10s)......\r\n");
    delay_1ms(3000);
    my_printf(USART1, "wait for start Application(7s)......\r\n");
    delay_1ms(3000);
    my_printf(USART1, "wait for start Application(4s)......\r\n");
    delay_1ms(3000);
    my_printf(USART1, "wait for start Application(1s)......\r\n");
    delay_1ms(1000);
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
            bootloader_wait_cmd();
            delay_1ms(5000);
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
