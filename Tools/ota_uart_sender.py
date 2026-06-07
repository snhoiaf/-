'''
FilePath: ota_uart_sender.py
Author: Ahypnis
Date: 2026-04-29 06:37:25
LastEditors: Please set LastEditors
LastEditTime: 2026-04-29 06:37:33
Copyright: 2026 0668STO CO.,LTD. All Rights Reserved.
'''
#!/usr/bin/env python3
import argparse
import binascii
import struct
import sys
import time

try:
    import serial
except ImportError:
    print("Missing dependency: pyserial")
    print("Install with: pip install pyserial")
    sys.exit(1)


MAGIC = 0x5AA5C33C
FRAME_HEADER = "<IBBHIHHI"
FRAME_HEADER_SIZE = struct.calcsize(FRAME_HEADER)
TYPE_START = 0x01
TYPE_DATA = 0x02
TYPE_END = 0x03
TYPE_ACK = 0x81
TYPE_NACK = 0x82
MAX_PAYLOAD = 512
APP1_START_ADDR = 0x08011000
APP_MAX_SIZE = 0x00020000
IMAGE_TYPE_APP1 = 1
HW_ID_ANY = 0


def parse_int(text: str) -> int:
    return int(text, 0)


def main() -> int:
    parser = argparse.ArgumentParser(description="GD32 OTA UART sender")
    parser.add_argument("--port", required=True, help="Serial port, e.g. COM5")
    parser.add_argument("--baud", type=int, default=921600, help="Baud rate")
    parser.add_argument("--bin", required=True, help="Firmware bin path")
    parser.add_argument("--version", type=parse_int, default=0x00010000, help="App version (supports 0x...)")
    parser.add_argument("--header-version", choices=("v1", "v2"), default="v1", help="START payload header version")
    parser.add_argument("--target-addr", type=parse_int, default=APP1_START_ADDR, help="v2 target address")
    parser.add_argument("--image-type", type=parse_int, default=IMAGE_TYPE_APP1, help="v2 image type")
    parser.add_argument("--hw-id", type=parse_int, default=HW_ID_ANY, help="v2 hardware id")
    parser.add_argument("--chunk", type=int, default=256, help="Payload bytes per frame")
    parser.add_argument("--ack-timeout", type=float, default=3.0, help="ACK timeout per frame (s)")
    parser.add_argument("--start-timeout", type=float, default=15.0, help="START ACK timeout, includes flash erase (s)")
    parser.add_argument("--retries", type=int, default=8, help="Retries per frame")
    parser.add_argument("--delay-ms", type=float, default=0.0, help=argparse.SUPPRESS)
    parser.add_argument("--erase-wait-ms", type=float, default=0.0, help=argparse.SUPPRESS)
    parser.add_argument("--tail-wait", type=float, default=2.0, help="Wait time after send (s)")
    parser.add_argument("--write-timeout", type=float, default=1.0, help="Serial write timeout (s)")
    parser.add_argument("--monitor-seconds", type=float, default=0.0, help="Read UART logs after send")
    args = parser.parse_args()

    with open(args.bin, "rb") as f:
        payload = f.read()

    if not payload:
        print("Firmware is empty")
        return 1

    app_size = len(payload)
    if app_size > APP_MAX_SIZE:
        print(f"Firmware too large: {app_size} bytes > {APP_MAX_SIZE} bytes")
        return 1

    app_crc32 = binascii.crc32(payload) & 0xFFFFFFFF
    if args.header_version == "v2":
        header_without_crc = struct.pack(
            "<IHHIIIIII",
            MAGIC,
            2,
            struct.calcsize("<IHHIIIIIII"),
            args.version,
            app_size,
            app_crc32,
            args.target_addr,
            args.image_type,
            args.hw_id,
        )
        header_crc32 = binascii.crc32(header_without_crc) & 0xFFFFFFFF
        app_header = header_without_crc + struct.pack("<I", header_crc32)
    else:
        app_header = struct.pack("<IIII", MAGIC, args.version, app_size, app_crc32)

    print(f"port      : {args.port}")
    print(f"baud      : {args.baud}")
    print(f"firmware  : {args.bin}")
    print(f"version   : 0x{args.version:08X}")
    print(f"size      : {app_size} bytes")
    print(f"crc32     : 0x{app_crc32:08X}")
    print(f"chunk     : {args.chunk} bytes")
    print(f"header    : {args.header_version}")

    chunk = max(1, min(args.chunk, MAX_PAYLOAD))
    magic_bytes = struct.pack("<I", MAGIC)
    rx_buf = bytearray()
    device_text = ""
    flow_paused = False

    def make_frame(frame_type: int, seq: int, offset: int, body: bytes = b"", crc_override=None) -> bytes:
        crc = (binascii.crc32(body) & 0xFFFFFFFF) if crc_override is None else crc_override
        header = struct.pack(FRAME_HEADER, MAGIC, frame_type, 0, seq & 0xFFFF,
                             offset & 0xFFFFFFFF, len(body), 0, crc & 0xFFFFFFFF)
        return header + body

    def handle_device_text(data: bytes):
        nonlocal device_text, flow_paused
        text = data.decode("utf-8", errors="ignore")
        if not text:
            return

        device_text += text.replace("\r", "\n")
        lines = device_text.split("\n")
        device_text = lines.pop()

        for line in lines:
            line = line.strip()
            if not line:
                continue
            tokens = line.split()
            if "PAUSE" in tokens:
                flow_paused = True
            if "RESUME" in tokens:
                flow_paused = False
            print(f"Device: {line}")

    def consume_text_from_rx_buffer():
        consumed = False

        while rx_buf:
            magic_pos = rx_buf.find(magic_bytes)
            if magic_pos < 0:
                if b"\r" not in rx_buf and b"\n" not in rx_buf and len(rx_buf) < FRAME_HEADER_SIZE:
                    break
                handle_device_text(bytes(rx_buf))
                rx_buf.clear()
                consumed = True
                break
            if magic_pos > 0:
                handle_device_text(bytes(rx_buf[:magic_pos]))
                del rx_buf[:magic_pos]
                consumed = True
                continue
            break

        return consumed

    def wait_flow_resume(ser, timeout: float):
        deadline = time.time() + max(timeout, 0.1)
        while flow_paused:
            n = ser.in_waiting
            if n:
                rx_buf.extend(ser.read(n))
                consume_text_from_rx_buffer()
            if not flow_paused:
                return
            if time.time() >= deadline:
                raise TimeoutError("flow control pause timeout")
            time.sleep(0.01)

    def wait_reply(ser, expect_seq: int, deadline: float):
        while time.time() < deadline:
            n = ser.in_waiting
            if n:
                rx_buf.extend(ser.read(n))
            consume_text_from_rx_buffer()

            while len(rx_buf) >= FRAME_HEADER_SIZE:
                magic_pos = rx_buf.find(magic_bytes)
                if magic_pos < 0:
                    consume_text_from_rx_buffer()
                    break
                if magic_pos > 0:
                    handle_device_text(bytes(rx_buf[:magic_pos]))
                    del rx_buf[:magic_pos]
                    continue
                magic, rtype, status, seq, offset, length, _reserved, crc = struct.unpack(
                    FRAME_HEADER, bytes(rx_buf[:FRAME_HEADER_SIZE])
                )
                if magic != MAGIC:
                    del rx_buf[0]
                    continue
                if length != 0:
                    if len(rx_buf) < FRAME_HEADER_SIZE + length:
                        break
                    del rx_buf[:FRAME_HEADER_SIZE + length]
                    continue
                del rx_buf[:FRAME_HEADER_SIZE]
                if seq != (expect_seq & 0xFFFF):
                    continue
                if rtype == TYPE_ACK:
                    return True, status, offset
                if rtype == TYPE_NACK:
                    return False, status, offset

            if not n:
                time.sleep(0.01)
        return None, None, None

    def send_with_ack(ser, frame: bytes, seq: int, label: str, timeout=None):
        ack_timeout = max(args.ack_timeout if timeout is None else timeout, 0.1)
        for attempt in range(1, max(args.retries, 0) + 2):
            wait_flow_resume(ser, max(ack_timeout, args.start_timeout))
            ser.write(frame)
            ser.flush()
            ok, status, offset = wait_reply(ser, seq, time.time() + ack_timeout)
            if ok is True:
                return offset
            if ok is False:
                print(f"{label}: NACK reason={status} device_offset={offset} retry={attempt}")
            else:
                print(f"{label}: ACK timeout retry={attempt}")
        raise TimeoutError(f"{label}: failed after retries")

    try:
        print(f"opening   : {args.port}")
        with serial.Serial(args.port, args.baud, timeout=1, write_timeout=args.write_timeout) as ser:
            print("serial    : opened")
            ser.reset_input_buffer()
            ser.reset_output_buffer()
            time.sleep(0.05)

            send_with_ack(ser, make_frame(TYPE_START, 0, 0, app_header), 0, "START", args.start_timeout)
            print("start ack : received")

            sent = 0
            seq = 1
            while sent < app_size:
                chunk_size = min(chunk, app_size - sent)
                chunk_data = payload[sent:sent + chunk_size]
                ack_offset = send_with_ack(
                    ser,
                    make_frame(TYPE_DATA, seq, sent, chunk_data),
                    seq,
                    f"DATA seq={seq} off={sent}",
                )
                expected_offset = sent + chunk_size
                if ack_offset != expected_offset:
                    print(f"bad ACK offset: got {ack_offset}, expected {expected_offset}")
                    return 3
                sent += chunk_size
                seq = (seq + 1) & 0xFFFF
                if sent % 4096 == 0 or sent == app_size:
                    print(f"sent      : {sent}/{app_size} ({sent*100//app_size}%)")

            send_with_ack(ser, make_frame(TYPE_END, seq, app_size, b"", app_crc32), seq, "END")
            print("send done, waiting device reset...")
            time.sleep(max(args.tail_wait, 0.0))

            if args.monitor_seconds > 0:
                end_time = time.time() + args.monitor_seconds
                print(f"monitor   : {args.monitor_seconds:.1f}s")
                while time.time() < end_time:
                    n = ser.in_waiting
                    if n > 0:
                        raw = ser.read(n)
                        try:
                            text = raw.decode("utf-8", errors="replace")
                        except Exception:
                            text = repr(raw)
                        print(text, end="", flush=True)
                    else:
                        time.sleep(0.02)
    except serial.SerialTimeoutException:
        print("serial write timeout: target may not be receiving or flow control is blocked")
        return 4
    except TimeoutError as e:
        print(str(e))
        return 6
    except serial.SerialException as e:
        print(f"serial error: {e}")
        return 5

    print("finished")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
