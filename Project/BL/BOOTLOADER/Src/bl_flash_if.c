/*
 * BL flash接口
 * 2026-05 重写
 */
#include "gd32f4xx.h"
#include "bl_config.h"
#include "bl_flash_if.h"
#include <string.h>

static void fl_clr(void)
{
    fmc_flag_clear(FMC_FLAG_END);
    fmc_flag_clear(FMC_FLAG_WPERR);
    fmc_flag_clear(FMC_FLAG_PGSERR);
    fmc_flag_clear(FMC_FLAG_PGMERR);
}

static int fl_wait(void)
{
    uint32_t t = 0x3FFFFFUL;
    while(RESET != fmc_flag_get(FMC_FLAG_BUSY) && t) t--;
    return t ? 1 : 0;
}

bool bl_flash_erase(uint32_t addr, uint32_t sz)
{
    uint32_t pg, end;
    if(!sz || addr < BL_FLASH_BASE_ADDR) return false;
    end = addr + sz - 1;
    if(end > BL_FLASH_END_ADDR) return false;
    pg  = addr - (addr % BL_FLASH_PAGE_SIZE);
    end = end - (end % BL_FLASH_PAGE_SIZE);
    fmc_unlock();
    fl_clr();
    while(pg <= end){
        fmc_page_erase(pg);
        if(!fl_wait()){ fmc_lock(); return false; }
        fl_clr();
        pg += BL_FLASH_PAGE_SIZE;
    }
    fmc_lock();
    return true;
}

bool bl_flash_program(uint32_t addr, const uint8_t *d, size_t n)
{
    uint32_t w;
    if(!d || !n) return false;
    if(addr < BL_FLASH_BASE_ADDR || addr + n - 1 > BL_FLASH_END_ADDR) return false;
    fmc_unlock();
    fl_clr();
    while(!(addr & 3) && n >= 4){
        memcpy(&w, d, 4);
        fmc_word_program(addr, w);
        if(!fl_wait()){ fmc_lock(); return false; }
        addr += 4; d += 4; n -= 4;
    }
    while(n--){
        fmc_byte_program(addr, *d);
        if(!fl_wait()){ fmc_lock(); return false; }
        addr++; d++;
    }
    fl_clr();
    fmc_lock();
    return true;
}

bool bl_flash_program_param(const void *p, size_t n)
{
    if(!p || !n || n > BL_PARAM_SIZE) return false;
    if(!bl_flash_erase(BL_PARAM_START_ADDR, BL_PARAM_SIZE)) return false;
    return bl_flash_program(BL_PARAM_START_ADDR, (const uint8_t*)p, n);
}
