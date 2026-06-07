#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
I/J 阶段测试：告警功能 + 睡眠唤醒
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

print("=" * 70)
print("I/J 阶段测试命令")
print("=" * 70)

print("\n### I 阶段：告警功能（7分）\n")

print("**I-01: 查询告警记录（已有CH0告警）**")
print(make_frame(0x0001, 0x0602, []))
print("预期：返回最近10条告警记录（倒序），字符串格式\n")

print("**I-02: 设置告警模式为'仅存储'（0x02）**")
print(make_frame(0x0001, 0x0601, [0x02]))
print("预期：收到OK，后续超阈值只存储不上报\n")

print("**I-03: 调高CH0阈值到10.0V（避免告警）**")
payload = float_to_bytes(10.0)
print(make_frame(0x0001, 0x0411, payload))
print("预期：收到OK\n")

print("**I-04: 调低CH0阈值到2.0V（触发告警）**")
payload = float_to_bytes(2.0)
print(make_frame(0x0001, 0x0411, payload))
print("预期：收到OK，因为模式=仅存储，不会发告警字符串\n")

print("**I-05: 查询告警记录（应该新增一条）**")
print(make_frame(0x0001, 0x0602, []))
print("预期：返回告警记录，包含刚才的2.0V告警\n")

print("**I-06: 设置告警模式为'主动上报'（0x01）**")
print(make_frame(0x0001, 0x0601, [0x01]))
print("预期：收到OK\n")

print("**I-07: 再次调低阈值到1.5V（触发告警+主动上报）**")
payload = float_to_bytes(1.5)
print(make_frame(0x0001, 0x0411, payload))
print("预期：收到OK，且立即发送告警字符串（格式：时间 | CH0 | 1.50 | 实际值）\n")

print("**I-08: 清除告警记录**")
print(make_frame(0x0001, 0x0603, []))
print("预期：收到OK\n")

print("**I-09: 查询告警记录（应为空）**")
print(make_frame(0x0001, 0x0602, []))
print("预期：返回字符串 'empty\\r\\n'\n")

print("\n" + "=" * 70)
print("### J 阶段：睡眠唤醒（5分）\n")

print("**J-01: 发送睡眠命令**")
print(make_frame(0x0001, 0x03AA, []))
print("预期：收到OK后，设备进入Stop模式深度睡眠\n")

print("**J-02: 等待10秒后自动唤醒**")
print("预期：串口输出字符串 'instrument wakeup\\r\\n'\n")

print("**J-03: 唤醒后发送查询命令验证通信**")
print(make_frame(0x0001, 0x0111, []))
print("预期：正常返回设备ID\n")

print("\n" + "=" * 70)
print("测试注意事项：")
print("- I阶段需要观察告警字符串格式")
print("- J阶段需要计时验证10秒唤醒")
print("=" * 70)
