# V2BL - GD32F470 BootLoader + UART OTA

## 硬件平台
- **主控**: GD32F470VET6 (Cortex-M4, 240MHz, 512KB Flash, 256KB SRAM)
- **公司**: MCUSTUDIO | **作者**: Ahypnis | **版本**: V0.10
- **开发环境**: Keil AC6 + CMSIS 6.2.0
- **工具链**: ARM Compiler 6 (armclang)

## 项目整体思路
BootLoader + App + 固件暂存区OTA方案：App运行时通过UART接收新固件写入固件暂存区域（历史兼容名APP2），复位后BootLoader校验暂存固件，拷贝到App区（历史兼容名APP1）后跳转运行。

## 分层架构

```
┌─────────────────────────────────────────────┐
│              APP 层 (应用任务)               │
│  led_app / adc_app / oled_app / btn_app     │
│  rtc_app / usart_app / sd_app               │
├─────────────────────────────────────────────┤
│           调度器 (scheduler.c)               │
│  基于时间片的协作式任务调度                    │
├─────────────────────────────────────────────┤
│           组件层 (Components)                │
│  ota_uart / ringbuffer / ebtn / fatfs       │
│  gd25qxx / gd30ad3344 / oled / sdio         │
├─────────────────────────────────────────────┤
│           BSP 层 (板级支持)                  │
│  mcu_cmic_gd32f470vet6.c/h                  │
│  引脚映射 / 外设初始化 / DMA配置              │
├─────────────────────────────────────────────┤
│         GD32F4xx 标准外设库                   │
│  gd32f4xx_gpio / usart / spi / dma / fmc    │
├─────────────────────────────────────────────┤
│           CMSIS / Cortex-M4                  │
└─────────────────────────────────────────────┘
```

## 工程结构

| 工程 | 路径 | 职责 |
|------|------|------|
| APP | `Project/APP/` | 主应用程序，含BSP+组件+任务调度 |
| BL | `Project/BL/` | BootLoader，参数校验/暂存固件拷贝/跳转 |

## Flash 分区 (不可改动)

| 区域 | 地址 | 大小 | 用途 |
|------|------|------|------|
| BootLoader | `0x08000000 - 0x0800FFFF` | 64KB | BL本体 |
| 参数页 | `0x08010000 - 0x08010FFF` | 4KB | 主参数+副本+日志 |
| App | `0x08011000 - 0x08030FFF` | 128KB | 当前运行App（历史兼容名APP1） |
| App备份 | `0x08031000 - 0x08050FFF` | 128KB | 预留备份区，当前未接入回滚 |
| 固件暂存 | `0x08051000 - 0x08070FFF` | 128KB | OTA下载暂存区（历史兼容名APP2） |
| DATA | `0x0807D000 - 0x0807FFFF` | 12KB | 预留用户数据 |

## 不可改动核心区域
- `common/bl_partition.h` — Flash分区地址定义
- `common/bl_param.h` — 参数页结构体与魔数
- `Components/ota_uart/ota_protocol.h` — OTA帧协议定义
- `BL/BOOTLOADER/` — BootLoader核心逻辑
- BSP层引脚映射（`mcu_cmic_gd32f470vet6.h` 中 `V2 pin map` 区域）

## 调度器任务表

| 任务 | 周期(ms) | 功能 |
|------|----------|------|
| led_task | 1 | LED状态刷新 |
| adc_task | 100 | ADC采样值更新 |
| oled_task | 10 | OLED双行显示：队伍编号+状态(IDLE/AutoSample) |
| btn_task | 5 | 按键扫描(ebtn) |
| uart_task | 5 | 串口数据处理+OTA |
