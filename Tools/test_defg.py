#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
D/E/F/G 阶段测试：变比+阈值+持久化
"""

import struct

def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1: crc = (crc>>1)^0xA001
            else: crc >>= 1
    return crc

def make_frame(dev_id, cmd, payload):
    frame = [0xA5, 0xB6]
    frame += [(dev_id>>8)&0xFF, dev_id&0xFF]
    frame += [0x01, (cmd>>8)&0xFF, cmd&0xFF]
    frame += [len(payload), 0x02] + payload
    crc = crc16(frame)
    frame += [(crc>>8)&0xFF, crc&0xFF, 0xB6, 0xA5]
    return ''.join('%02X' % b for b in frame)

def float_to_bytes(f):
    return list(struct.pack('>f', f))

def bytes_to_float(b):
    return struct.unpack('>f', bytes(b))[0]

print("=" * 70)
print("D/E/F/G 阶段测试命令")
print("=" * 70)

print("\n### D 阶段：设置变比（4分）\n")

print("**D-01: 设置CH0变比为2.5**")
ch0_ratio = 2.5
payload = float_to_bytes(ch0_ratio)
print(make_frame(0x0001, 0x0241, payload))
print(f"预期：收到OK（变比={ch0_ratio}）\n")

print("**D-02: 设置CH1变比为3.0**")
ch1_ratio = 3.0
payload = float_to_bytes(ch1_ratio)
print(make_frame(0x0001, 0x0242, payload))
print(f"预期：收到OK（变比={ch1_ratio}）\n")

print("**D-03: 查询CH0（应乘以变比2.5）**")
print(make_frame(0x0001, 0x0201, []))
print("预期：返回值 = 原始ADC值 × 2.5\n")

print("**D-04: 查询CH1（应乘以变比3.0）**")
print(make_frame(0x0001, 0x0202, []))
print("预期：返回值 = DAC回读值 × 3.0\n")

print("\n" + "=" * 70)
print("### E 阶段：设置阈值（4分）\n")

print("**E-01: 写CH0阈值为5.0V**")
ch0_thr = 5.0
payload = float_to_bytes(ch0_thr)
print(make_frame(0x0001, 0x0411, payload))
print(f"预期：收到OK（阈值={ch0_thr}V）\n")

print("**E-02: 写CH1阈值为6.0V**")
ch1_thr = 6.0
payload = float_to_bytes(ch1_thr)
print(make_frame(0x0001, 0x0412, payload))
print(f"预期：收到OK（阈值={ch1_thr}V）\n")

print("**E-03: 读CH0阈值**")
print(make_frame(0x0001, 0x0401, []))
print(f"预期：返回IEEE754浮点 = {ch0_thr}V\n")

print("**E-04: 读CH1阈值**")
print(make_frame(0x0001, 0x0402, []))
print(f"预期：返回IEEE754浮点 = {ch1_thr}V\n")

print("\n" + "=" * 70)
print("### F/G 阶段：持久化验证（11分）\n")

print("**F/G-01: 重启设备**")
print(make_frame(0x0001, 0x8888, []))
print("预期：收到OK后复位\n")

print("**F/G-02: 重启后读CH0阈值（验证持久化）**")
print(make_frame(0x0001, 0x0401, []))
print(f"预期：仍为 {ch0_thr}V（持久化成功）\n")

print("**F/G-03: 重启后读CH1阈值（验证持久化）**")
print(make_frame(0x0001, 0x0402, []))
print(f"预期：仍为 {ch1_thr}V（持久化成功）\n")

print("**F/G-04: 重启后查CH0（验证变比持久化）**")
print(make_frame(0x0001, 0x0201, []))
print(f"预期：仍乘以变比2.5（持久化成功）\n")

print("**F/G-05: 重启后查CH1（验证变比持久化）**")
print(make_frame(0x0001, 0x0202, []))
print(f"预期：仍乘以变比3.0（持久化成功）\n")

print("\n" + "=" * 70)
print("测试完成后所有配置应持久化到Flash")
print("=" * 70)
