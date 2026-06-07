# GD32F470 V2 BootLoader + UART OTA

本目录是 V2 硬件的 BootLoader OTA 示例，工具链为 AC6 + CMSIS6。它包含一个 BootLoader 工程、一个可 OTA 的 App 工程、测试固件和 PC 端串口发送脚本。

当前升级方式为 BootLoader + App + 固件暂存区：App 在运行时通过 UART 接收新固件并写入固件暂存区域（历史兼容名APP2），复位后由 BootLoader 校验暂存固件，再拷贝到 App 区（历史兼容名APP1）并跳转运行。

## 目录说明

| 目录 | 内容 |
| --- | --- |
| `Firmware/Test_App_Bin/` | OTA 测试用 App bin，例如 `App_ON.bin`。 |
| `Project/BL/` | BootLoader 工程，负责参数页恢复、升级状态判断、暂存固件校验、暂存固件到 App 区拷贝、App 区校验和跳转。 |
| `Project/APP/` | App 工程，包含板级驱动、应用任务和 `ota_uart` 接收组件。 |
| `Tools/ota_uart_sender.py` | PC 侧 OTA 发送脚本。 |

## Flash 分区

分区定义在 BootLoader 和 App 工程的 `common/bl_partition.h` 中，两边必须保持一致。

| 区域 | 地址范围 | 大小 | 用途 |
| --- | --- | --- | --- |
| BootLoader | `0x08000000` - `0x0800FFFF` | 64 KB | BootLoader 本体。 |
| 参数页 | `0x08010000` - `0x08010FFF` | 4 KB | 主参数、副本参数和 BootLoader 日志。 |
| App | `0x08011000` - `0x08030FFF` | 128 KB | 当前运行 App（历史兼容名APP1）。 |
| App备份 | `0x08031000` - `0x08050FFF` | 128 KB | 预留备份区，当前未接入回滚。 |
| 固件暂存 | `0x08051000` - `0x08070FFF` | 128 KB | OTA 下载暂存区（历史兼容名APP2）。 |
| DATA | `0x0807D000` - `0x0807FFFF` | 12 KB | 预留用户数据区，位于照片未占用尾部空间。 |

关键配置：

- BootLoader scatter：`Project/BL/MDK/BootLoader_F470.sct`
- App scatter：`Project/APP/MDK/App_F470.sct`
- App 向量表：`USER/src/main.c` 中 `SCB->VTOR = BL_APP1_START_ADDR`
- PC 脚本默认目标地址：`--target-addr 0x08011000`

## 编译和烧录

1. 打开 BootLoader 工程：`Project/GD_BootLoader_F470_ac6_cmsis_6/MDK/Project.uvprojx`
2. 确认 `BootLoader_F470.sct` 的 `LR_IROM1` 为 `0x08000000 0x00010000`
3. 编译并下载 BootLoader 到 `0x08000000`
4. 打开 App 工程：`Project/GD_Firmware_Template_ac6_cmsis_6_with_dependence/MDK/Project.uvprojx`
5. 确认 `App_F470.sct` 的 `LR_IROM1` 为 `0x08011000 0x00020000`
6. 编译 App，生成 `MDK/output/Project.hex` 和 `MDK/output/App.bin`
7. 首次调试可将 App hex 下载到 App 区 `0x08011000`；后续 OTA 使用 `App.bin`

使用 Keil 分别下载两个工程时，下载 App 不要选择整片擦除，否则会擦掉 BootLoader 和参数页。应使用按扇区或按需擦除。

## 串口 OTA

V2 App 工程的 OTA 串口配置在 `Components/bsp/mcu_cmic_gd32f470vet6.h`：

| 项 | 配置 |
| --- | --- |
| OTA UART | `USART2` |
| 引脚 | `PD8` 为 TX，`PD9` 为 RX |
| DMA | `DMA0 CH1 SUB4` |
| 波特率 | `921600` |
| 接收缓冲 | `2048` 字节 |
| Debug UART | `USART0`，`PA9/PA10`，`115200` |

安装 PC 端依赖：

```powershell
pip install pyserial
```

从本目录执行 OTA：

```powershell
python Tools\ota_uart_sender.py --port COM4 --baud 921600 --bin Project\APP\MDK\output\App.bin --header-version v2 --target-addr 0x08011000 --monitor-seconds 5
```

也可以发送测试固件：

```powershell
python Tools\ota_uart_sender.py --port COM4 --baud 921600 --bin Firmware\Test_App_Bin\App_ON.bin --version 0x00010001 --header-version v2 --target-addr 0x08011000
```

## 常见检查点

- `App.bin` 不能超过 `0x00020000` 字节。
- BootLoader、App 和 Python 脚本中的 App 起始地址必须都是 `0x08011000`。
- BootLoader 和 App 的 `common/bl_partition.h`、`common/bl_param.h` 必须同步。
- START 阶段超时优先检查串口接线、波特率和 App 是否已运行。
- END 后未进入新 App 时，查看 debug 串口中的 APP2 CRC、APP1 CRC 和向量表校验日志。
