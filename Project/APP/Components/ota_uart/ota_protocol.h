#ifndef __OTA_PROTOCOL_H
#define __OTA_PROTOCOL_H

#include <stdint.h>

#if defined(__CC_ARM)
#define OTA_PACKED  __packed struct
#else
#define OTA_PACKED  struct __attribute__((packed))
#endif

/* 帧同步字 */
#define OTA_SYNC        0x5AA5C33CUL

/* 帧类型 */
#define OTA_FT_START    0x01
#define OTA_FT_DATA     0x02
#define OTA_FT_END      0x03
#define OTA_FT_ACK      0x81
#define OTA_FT_NACK     0x82

/* NACK原因 */
#define OTA_NK_STATE    1
#define OTA_NK_CRC      2
#define OTA_NK_SEQ      3
#define OTA_NK_OFFSET   4
#define OTA_NK_LEN      5
#define OTA_NK_FLASH    6
#define OTA_NK_FINAL    7

/* 单帧最大负载 */
#define OTA_PL_MAX      512

/* v2镜像头版本号 */
#define OTA_HDR_V2_VER  2
#define OTA_IMG_TYPE_APP 1

/*
 * 固定帧头，20字节
 * crc32只校验payload，不校验帧头本身
 */
typedef OTA_PACKED {
    uint32_t sync;
    uint8_t  ft;
    uint8_t  st;
    uint16_t seq;
    uint32_t off;
    uint16_t plen;
    uint16_t _rsv;
    uint32_t fcrc;
} ota_frm_t;

/* v1 START载荷，兼容老发送器 */
typedef OTA_PACKED {
    uint32_t sync;
    uint32_t ver;
    uint32_t size;
    uint32_t crc;
} ota_meta_t;

/* v2 START载荷，带目标地址校验 */
typedef OTA_PACKED {
    uint32_t sync;
    uint16_t hdr_ver;
    uint16_t hdr_sz;
    uint32_t ver;
    uint32_t size;
    uint32_t crc;
    uint32_t dst;
    uint32_t img_type;
    uint32_t hw_id;
    uint32_t hdr_crc;
} ota_meta_v2_t;

#endif
