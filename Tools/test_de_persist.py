#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
D/E/F/G 阶段持久化自动化测试脚本
测试变比、阈值设置及重启后持久化
"""

import serial
import struct
import time
import sys
import io

# Windows终端GBK编码，重定向stdout到UTF-8
if sys.platform == 'win32':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

# 配置
COM_PORT = 'COM12'
BAUDRATE = 19200
TIMEOUT = 2.0

def crc16_modbus(data):
    """CRC16-Modbus"""
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc

def make_frame(dev_id, cmd, payload):
    """生成协议帧"""
    frame = [0xA5, 0xB6]
    frame += [(dev_id >> 8) & 0xFF, dev_id & 0xFF]
    frame += [0x01, (cmd >> 8) & 0xFF, cmd & 0xFF]
    frame += [len(payload), 0x02] + payload
    crc = crc16_modbus(frame)
    frame += [(crc >> 8) & 0xFF, crc & 0xFF, 0xB6, 0xA5]
    return bytes(frame)

def float_to_bytes_be(f):
    """浮点转大端字节"""
    return list(struct.pack('>f', f))

def bytes_to_float_be(b):
    """大端字节转浮点"""
    return struct.unpack('>f', bytes(b))[0]

def send_and_recv(ser, frame, desc, timeout=2.0):
    """发送帧并接收响应"""
    print(f"\n{'='*60}")
    print(f"[TEST] {desc}")
    print(f"[SEND] {frame.hex().upper()}")

    ser.reset_input_buffer()
    ser.write(frame)

    time.sleep(0.2)  # 等待设备处理

    resp = ser.read(256)
    if resp:
        print(f"[RECV] {resp.hex().upper()}")
        return resp
    else:
        print(f"[RECV] 无响应")
        return None

def parse_ok_frame(resp):
    """解析OK帧"""
    if not resp or len(resp) < 13:
        return False
    if resp[0:2] != b'\xA5\xB6':
        return False
    if resp[4] != 0x02:  # 类型应答
        return False
    payload_len = resp[7]
    if payload_len == 1 and resp[9] == 0xFF:
        return True
    return False

def parse_float_frame(resp, expected_len=4):
    """解析浮点数据帧"""
    if not resp or len(resp) < 13:
        return None
    payload_len = resp[7]
    if payload_len != expected_len:
        return None
    payload = resp[9:9+payload_len]
    if expected_len == 4:
        return bytes_to_float_be(payload)
    elif expected_len == 8:
        return [bytes_to_float_be(payload[0:4]), bytes_to_float_be(payload[4:8])]
    return None

def main():
    print("="*60)
    print("D/E/F/G 持久化自动化测试")
    print("="*60)

    try:
        ser = serial.Serial(COM_PORT, BAUDRATE, timeout=TIMEOUT)
        print(f"[INFO] 已打开 {COM_PORT} @ {BAUDRATE}")
    except Exception as e:
        print(f"[ERROR] 无法打开串口: {e}")
        print(f"[INFO] 请关闭占用 {COM_PORT} 的其他程序")
        return 1

    # 等待设备进入APP（如果在BL倒计时则等待跳转）
    print("[INFO] 等待设备进入APP...")
    ser.reset_input_buffer()
    time.sleep(2)

    # 发送心跳确认设备在APP状态
    for retry in range(3):
        frame = make_frame(0x0001, 0x0101, [])
        ser.write(frame)
        time.sleep(0.3)
        resp = ser.read(256)
        if resp and len(resp) >= 13 and resp[0:2] == b'\xA5\xB6':
            print(f"[INFO] 设备已在APP状态")
            break
        if retry < 2:
            print(f"[INFO] 未收到协议帧，等待BL跳转... ({retry+1}/3)")
            time.sleep(3)
    else:
        print("[WARN] 设备可能未进入APP，继续测试...")

    time.sleep(1)

    results = {}

    # Phase 1: 设置变比
    print("\n" + "="*60)
    print("Phase 1: 设置并验证变比（D-01/D-02）")
    print("="*60)

    # 1. 设CH0变比=21.59
    frame = make_frame(0x0001, 0x0241, float_to_bytes_be(21.59))
    resp = send_and_recv(ser, frame, "设CH0变比=21.59")
    results['set_ch0_ratio'] = parse_ok_frame(resp)
    print(f"[结果] {'✓ PASS' if results['set_ch0_ratio'] else '✗ FAIL'}")

    # 2. 设CH1变比=10.5
    frame = make_frame(0x0001, 0x0242, float_to_bytes_be(10.5))
    resp = send_and_recv(ser, frame, "设CH1变比=10.5")
    results['set_ch1_ratio'] = parse_ok_frame(resp)
    print(f"[结果] {'✓ PASS' if results['set_ch1_ratio'] else '✗ FAIL'}")

    # 3. 查CH0
    frame = make_frame(0x0001, 0x0201, [])
    resp = send_and_recv(ser, frame, "查CH0电压（应为原始×21.59）")
    ch0_before = parse_float_frame(resp, 4)
    results['query_ch0_before'] = ch0_before is not None
    if ch0_before:
        print(f"[数据] CH0 = {ch0_before:.3f} V")
    print(f"[结果] {'✓ PASS' if results['query_ch0_before'] else '✗ FAIL'}")

    # 4. 查CH1
    frame = make_frame(0x0001, 0x0202, [])
    resp = send_and_recv(ser, frame, "查CH1电压（应为DAC回读×10.5）")
    ch1_before = parse_float_frame(resp, 4)
    results['query_ch1_before'] = ch1_before is not None
    if ch1_before:
        print(f"[数据] CH1 = {ch1_before:.3f} V")
    print(f"[结果] {'✓ PASS' if results['query_ch1_before'] else '✗ FAIL'}")

    # Phase 2: 设置阈值
    print("\n" + "="*60)
    print("Phase 2: 设置并验证阈值（E-01~E-06）")
    print("="*60)

    # 5. 设CH0阈值=3.5
    frame = make_frame(0x0001, 0x0411, float_to_bytes_be(3.5))
    resp = send_and_recv(ser, frame, "设CH0阈值=3.5V")
    results['set_ch0_thr'] = parse_ok_frame(resp)
    print(f"[结果] {'✓ PASS' if results['set_ch0_thr'] else '✗ FAIL'}")

    # 6. 设CH1阈值=2.8
    frame = make_frame(0x0001, 0x0412, float_to_bytes_be(2.8))
    resp = send_and_recv(ser, frame, "设CH1阈值=2.8V")
    results['set_ch1_thr'] = parse_ok_frame(resp)
    print(f"[结果] {'✓ PASS' if results['set_ch1_thr'] else '✗ FAIL'}")

    # 7. 批量读阈值
    frame = make_frame(0x0001, 0x0400, [])
    resp = send_and_recv(ser, frame, "批量读阈值（应为3.5+2.8）")
    thr_before = parse_float_frame(resp, 8)
    results['query_thr_before'] = thr_before is not None
    if thr_before:
        print(f"[数据] CH0阈值={thr_before[0]:.2f}V, CH1阈值={thr_before[1]:.2f}V")
        results['thr_values_ok'] = abs(thr_before[0] - 3.5) < 0.01 and abs(thr_before[1] - 2.8) < 0.01
        print(f"[验证] 阈值准确性: {'✓ PASS' if results['thr_values_ok'] else '✗ FAIL'}")
    print(f"[结果] {'✓ PASS' if results['query_thr_before'] else '✗ FAIL'}")

    # Phase 3: 重启验证持久化
    print("\n" + "="*60)
    print("Phase 3: 重启验证持久化（F/G）")
    print("="*60)

    # 8. 重启
    frame = make_frame(0x0001, 0x8888, [])
    resp = send_and_recv(ser, frame, "发送重启命令0x8888")
    results['reboot'] = parse_ok_frame(resp)
    print(f"[结果] {'✓ PASS' if results['reboot'] else '✗ FAIL'}")

    print("\n[INFO] 等待设备重启...")
    time.sleep(5)

    # 清空重启期间的数据
    ser.reset_input_buffer()

    # 9. 重启后查CH0
    frame = make_frame(0x0001, 0x0201, [])
    resp = send_and_recv(ser, frame, "重启后查CH0（验证变比持久化）")
    ch0_after = parse_float_frame(resp, 4)
    results['query_ch0_after'] = ch0_after is not None
    if ch0_after and ch0_before:
        print(f"[数据] 重启前CH0={ch0_before:.3f}V, 重启后CH0={ch0_after:.3f}V")
        results['ch0_persist'] = abs(ch0_after - ch0_before) < 0.01
        print(f"[验证] CH0变比持久化: {'✓ PASS' if results['ch0_persist'] else '✗ FAIL'}")
    print(f"[结果] {'✓ PASS' if results['query_ch0_after'] else '✗ FAIL'}")

    # 10. 重启后批量读阈值
    frame = make_frame(0x0001, 0x0400, [])
    resp = send_and_recv(ser, frame, "重启后批量读阈值（验证阈值持久化）")
    thr_after = parse_float_frame(resp, 8)
    results['query_thr_after'] = thr_after is not None
    if thr_after and thr_before:
        print(f"[数据] 重启前阈值=[{thr_before[0]:.2f}, {thr_before[1]:.2f}]")
        print(f"[数据] 重启后阈值=[{thr_after[0]:.2f}, {thr_after[1]:.2f}]")
        results['thr_persist'] = (abs(thr_after[0] - thr_before[0]) < 0.01 and
                                   abs(thr_after[1] - thr_before[1]) < 0.01)
        print(f"[验证] 阈值持久化: {'✓ PASS' if results['thr_persist'] else '✗ FAIL'}")
    print(f"[结果] {'✓ PASS' if results['query_thr_after'] else '✗ FAIL'}")

    ser.close()

    # 汇总报告
    print("\n" + "="*60)
    print("测试报告汇总")
    print("="*60)

    print("\n[D-01/D-02] 变比设置:")
    print(f"  设CH0变比: {'✓' if results.get('set_ch0_ratio') else '✗'}")
    print(f"  设CH1变比: {'✓' if results.get('set_ch1_ratio') else '✗'}")
    print(f"  查CH0生效: {'✓' if results.get('query_ch0_before') else '✗'}")
    print(f"  查CH1生效: {'✓' if results.get('query_ch1_before') else '✗'}")

    print("\n[E-01~E-06] 阈值设置:")
    print(f"  设CH0阈值: {'✓' if results.get('set_ch0_thr') else '✗'}")
    print(f"  设CH1阈值: {'✓' if results.get('set_ch1_thr') else '✗'}")
    print(f"  批量读阈值: {'✓' if results.get('query_thr_before') else '✗'}")
    print(f"  阈值准确性: {'✓' if results.get('thr_values_ok') else '✗'}")

    print("\n[F/G] 重启持久化:")
    print(f"  重启命令: {'✓' if results.get('reboot') else '✗'}")
    print(f"  CH0变比持久化: {'✓' if results.get('ch0_persist') else '✗'}")
    print(f"  阈值持久化: {'✓' if results.get('thr_persist') else '✗'}")

    passed = sum(1 for v in results.values() if v)
    total = len(results)

    print(f"\n[总计] {passed}/{total} 项通过")
    print("="*60)

    return 0 if passed == total else 1

if __name__ == '__main__':
    sys.exit(main())
