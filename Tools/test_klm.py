#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
K/L/M 阶段测试：异常帧处理 + 修改ID + 修改波特率
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

print("=" * 70)
print("K/L/M 阶段测试命令")
print("=" * 70)

print("\n### K 阶段：异常帧处理（6分）\n")

print("**K-01: CRC错误帧**")
print("发送：A5B60001010101000251FFFFB6A5  (故意错误的CRC)")
print("预期：A5B6 FFFF 02 EEEE 01 02 FF ... B6A5  (错误应答)")

print("\n**K-02: 长度错误帧**")
print("发送：A5B60001010201050251ABB6A5  (声明长度5，实际payload长度0)")
print("预期：A5B6 FFFF 02 EEEE 01 02 FF ... B6A5  (错误应答)")

print("\n**K-03: 非法命令字**")
print(make_frame(0x0001, 0x9999, []))
print("预期：A5B6 FFFF 02 EEEE 01 02 FF ... B6A5  (错误应答)")

print("\n" + "=" * 70)
print("### L 阶段：修改设备ID（6分）\n")

print("**L-01: 设置设备ID为0x0002**")
print(make_frame(0x0001, 0x01A1, [0x00, 0x02]))
print("预期：A5B6 0001 02 01A1 01 02 FF ... B6A5 (OK)")

print("\n**L-02: 用新ID 0x0002 查询设备ID**")
print(make_frame(0x0002, 0x0111, []))
print("预期：A5B6 0002 02 0111 02 02 0002 ... B6A5 (返回0x0002)")

print("\n**L-03: 重启设备**")
print(make_frame(0x0002, 0x8888, []))
print("预期：A5B6 0002 02 8888 01 02 FF ... B6A5 (OK，然后复位)")

print("\n**L-04: 重启后用新ID 0x0002 查询（验证持久化）**")
print(make_frame(0x0002, 0x0111, []))
print("预期：A5B6 0002 02 0111 02 02 0002 ... B6A5 (仍为0x0002)")

print("\n**L-05: 恢复ID为0x0001**")
print(make_frame(0x0002, 0x01A1, [0x00, 0x01]))
print("预期：A5B6 0002 02 01A1 01 02 FF ... B6A5 (OK)")

print("\n" + "=" * 70)
print("### M 阶段：修改波特率（6分）\n")

print("**M-01: 查询当前波特率（应为19200）**")
print(make_frame(0x0001, 0x0112, []))
print("预期：A5B6 0001 02 0112 01 02 11 ... B6A5 (0x11=19200)")

print("\n**M-02: 设置波特率为115200**")
print(make_frame(0x0001, 0x01A2, [0x14]))
print("预期：A5B6 0001 02 01A2 01 02 FF ... B6A5 (OK，然后复位)")
print("⚠️ 收到OK后，设备会在100ms后自动重启并切换到115200")

print("\n**M-03: 切换串口工具波特率到115200，重启后查询波特率**")
print(make_frame(0x0001, 0x0112, []))
print("预期：A5B6 0001 02 0112 01 02 14 ... B6A5 (0x14=115200)")

print("\n**M-04: 恢复波特率为19200**")
print(make_frame(0x0001, 0x01A2, [0x11]))
print("预期：A5B6 0001 02 01A2 01 02 FF ... B6A5 (OK，然后复位)")
print("⚠️ 收到OK后切换串口工具回19200")

print("\n" + "=" * 70)
print("测试完成，按步骤在串口工具中执行")
print("=" * 70)
