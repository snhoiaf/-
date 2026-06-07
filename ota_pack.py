#!/usr/bin/env python3
"""
OTA固件打包工具
生成符合ota_protocol.h格式的升级包
"""
import struct
import sys
from pathlib import Path

# 协议常量
OTA_SYNC = 0x5AA5C33C
OTA_FT_START = 0x01
OTA_FT_DATA = 0x02
OTA_FT_END = 0x03

OTA_HDR_V2_VER = 2
OTA_IMG_TYPE_APP = 1
BL_APP1_START_ADDR = 0x08011000  # APP起始地址

def crc32(data):
    """计算CRC32"""
    import zlib
    return zlib.crc32(data) & 0xFFFFFFFF

def pack_frame(ft, st, seq, off, payload):
    """打包OTA帧"""
    plen = len(payload)
    fcrc = crc32(payload) if payload else 0

    # 帧头：20字节
    header = struct.pack('<IBBHIHHI',
        OTA_SYNC,   # sync
        ft,         # ft
        st,         # st
        seq,        # seq
        off,        # off
        plen,       # plen
        0,          # _rsv
        fcrc        # fcrc
    )

    return header + payload

def generate_ota(bin_path, out_path):
    """生成OTA升级包"""
    # 读取固件
    with open(bin_path, 'rb') as f:
        firmware = f.read()

    fw_size = len(firmware)
    fw_crc = crc32(firmware)

    print(f"固件大小: {fw_size} bytes")
    print(f"固件CRC32: 0x{fw_crc:08X}")

    # 构造v2 meta头
    meta = struct.pack('<IHHIIIII',
        OTA_SYNC,           # sync
        OTA_HDR_V2_VER,     # hdr_ver
        32,                 # hdr_sz (meta头大小)
        0x00020000,         # ver (v2.0)
        fw_size,            # size
        fw_crc,             # crc
        BL_APP1_START_ADDR, # dst
        OTA_IMG_TYPE_APP    # img_type
    )
    # 计算meta头CRC (前28字节)
    meta_crc = crc32(meta[:28])
    meta += struct.pack('<I', meta_crc)

    frames = []

    # START帧
    start_frame = pack_frame(OTA_FT_START, 0, 0, 0, meta)
    frames.append(start_frame)
    print(f"START帧: {len(start_frame)} bytes")

    # DATA帧 (每帧256字节)
    seq = 1
    offset = 0
    CHUNK_SIZE = 256

    while offset < fw_size:
        chunk = firmware[offset:offset + CHUNK_SIZE]
        data_frame = pack_frame(OTA_FT_DATA, 0, seq, offset, chunk)
        frames.append(data_frame)
        offset += len(chunk)
        seq += 1

    print(f"DATA帧: {seq - 1} 帧")

    # END帧
    end_frame = pack_frame(OTA_FT_END, 0, seq, offset, b'')
    frames.append(end_frame)
    print(f"END帧: {len(end_frame)} bytes")

    # 写入OTA包
    with open(out_path, 'wb') as f:
        for frame in frames:
            f.write(frame)

    total_size = sum(len(f) for f in frames)
    print(f"OTA包生成成功: {out_path}")
    print(f"总大小: {total_size} bytes")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(f"用法: {sys.argv[0]} <App.bin> [output.ota]")
        sys.exit(1)

    bin_path = sys.argv[1]
    out_path = sys.argv[2] if len(sys.argv) > 2 else 'firmware.ota'

    generate_ota(bin_path, out_path)
