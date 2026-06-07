#ifndef __BL_FLASH_IF_H
#define __BL_FLASH_IF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

bool bl_flash_erase(uint32_t addr, uint32_t sz);
bool bl_flash_program(uint32_t addr, const uint8_t *d, size_t n);
bool bl_flash_program_param(const void *p, size_t n);

#endif
