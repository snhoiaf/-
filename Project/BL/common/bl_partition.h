/*
 * Flash分区地址定义
 * 按照片要求定义BootLoader、参数页、App、App备份、固件暂存和DATA区域
 */
#ifndef COMMON_BL_PARTITION_H
#define COMMON_BL_PARTITION_H

#include <stdint.h>

/* Flash总容量定义 */
#define BL_FLASH_BASE_ADDR          0x08000000UL
#define BL_FLASH_TOTAL_SIZE         0x00080000UL
#define BL_FLASH_END_ADDR           (BL_FLASH_BASE_ADDR + BL_FLASH_TOTAL_SIZE - 1UL)
#define BL_FLASH_PAGE_SIZE          0x00001000UL

/* BootLoader区域定义: 0x08000000 - 0x0800FFFF (64KB) */
#define BL_BOOT_START_ADDR          0x08000000UL
#define BL_BOOT_SIZE                0x00010000UL
#define BL_BOOT_END_ADDR            (BL_BOOT_START_ADDR + BL_BOOT_SIZE - 1UL)

/* 参数页区域定义: 0x08010000 - 0x08010FFF (4KB) */
#define BL_PARAM_START_ADDR         0x08010000UL
#define BL_PARAM_SIZE               0x00001000UL
#define BL_PARAM_END_ADDR           (BL_PARAM_START_ADDR + BL_PARAM_SIZE - 1UL)

/* App运行区域定义: 0x08011000 - 0x08030FFF (128KB) */
#define BL_APP_START_ADDR           0x08011000UL
#define BL_APP_SIZE                 0x00020000UL
#define BL_APP_END_ADDR             (BL_APP_START_ADDR + BL_APP_SIZE - 1UL)

/* App备份区域定义: 0x08031000 - 0x08050FFF (128KB)，当前预留 */
#define BL_APP_BACKUP_START_ADDR    0x08031000UL
#define BL_APP_BACKUP_SIZE          0x00020000UL
#define BL_APP_BACKUP_END_ADDR      (BL_APP_BACKUP_START_ADDR + BL_APP_BACKUP_SIZE - 1UL)

/* 固件暂存区域定义: 0x08051000 - 0x08070FFF (128KB) */
#define BL_OTA_STAGING_START_ADDR   0x08051000UL
#define BL_OTA_STAGING_SIZE         0x00020000UL
#define BL_OTA_STAGING_END_ADDR     (BL_OTA_STAGING_START_ADDR + BL_OTA_STAGING_SIZE - 1UL)

/* 历史兼容命名: APP1=当前运行App，APP2=OTA固件暂存区 */
#define BL_APP1_START_ADDR          BL_APP_START_ADDR
#define BL_APP1_SIZE                BL_APP_SIZE
#define BL_APP1_END_ADDR            BL_APP_END_ADDR

#define BL_APP2_START_ADDR          BL_OTA_STAGING_START_ADDR
#define BL_APP2_SIZE                BL_OTA_STAGING_SIZE
#define BL_APP2_END_ADDR            BL_OTA_STAGING_END_ADDR

/* 用户数据区域定义: 0x0807D000 - 0x0807FFFF (12KB) */
#define BL_DATA_START_ADDR          0x0807D000UL
#define BL_DATA_SIZE                0x00003000UL
#define BL_DATA_END_ADDR            (BL_DATA_START_ADDR + BL_DATA_SIZE - 1UL)

#endif /* COMMON_BL_PARTITION_H */
