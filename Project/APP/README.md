# GD32F470 V2 OTA App Project

这是 V2 BootLoader OTA 示例中的 App 工程，使用 AC6 + CMSIS6，并已携带工程依赖。App 从照片要求的 App 区运行，同时负责通过 UART 接收新固件并写入固件暂存区域（历史兼容名APP2）。

## 工程信息

| 项 | 内容 |
| --- | --- |
| 入口工程 | `MDK/Project.uvprojx` |
| Scatter | `MDK/App_F470.sct` |
| IROM | `0x08011000 0x00020000` |
| 向量表 | `USER/src/main.c` 中 `SCB->VTOR = BL_APP1_START_ADDR` |
| OTA 组件 | `Components/ota_uart/` |
| 环形缓冲 | `Components/ringbuffer/` |
| 共用分区 | `common/bl_partition.h` |
| 参数页 | `common/bl_param.h` |

## 目录说明

| 目录 | 内容 |
| --- | --- |
| `APP/` | 应用层示例：LED、按键、OLED、串口、SD 卡、ADC、RTC 和调度器。 |
| `Components/` | BSP、FatFs、外设驱动、`ota_uart` 和 `ringbuffer`。 |
| `common/` | 与 BootLoader 保持一致的分区和升级参数定义。 |
| `Driver/`、`Libraries/` | CMSIS6、GD32F4xx 标准外设库和启动文件。 |
| `PACK/` | 工程内置扩展包，例如 perf_counter 2.5.4。 |
| `USER/` | App 入口 `main.c`、中断和 SysTick。 |
| `MDK/` | Keil 工程、scatter、输出目录和 `fromelf` bin 生成配置。 |

## OLED 显示

APP 运行期间 OLED 固定双行文本显示：

| 行号 | 内容 |
| --- | --- |
| 第1行 | `2026055095` |
| 第2行 | 自动采样中显示 `AutoSample`，其余时刻显示 `IDLE` |

采样电压和 RTC 时间戳继续通过 USART1 上报，不再占用 OLED 第3/4行。

## OTA 串口配置

| 项 | 配置 |
| --- | --- |
| OTA UART | `USART2` |
| 引脚 | `PD8` TX，`PD9` RX |
| DMA | `DMA0 CH1 SUB4` |
| 波特率 | `921600` |
| RX 缓冲 | `2048` 字节 |
| Debug UART | `USART0`，`PA9/PA10`，`115200` |

## OTA 流程

1. PC 脚本发送 START、DATA、END 帧。
2. `ota_uart.c` 校验 START 头中的大小、CRC、目标地址和镜像类型。
3. App 擦除固件暂存区域（历史兼容名APP2），并按 seq / offset 顺序写入数据。
4. END 阶段校验总大小和 CRC32。
5. 校验通过后写入参数页：`BL_UPDATE_FLAG_PENDING`、`app_size`、`app_crc32`、APP1/APP2 地址。
6. App 调用 `NVIC_SystemReset()`，复位进入 BootLoader。
7. BootLoader 校验固件暂存区域，并拷贝到 App 区后运行新 App。

## 编译输出

编译后关注两个文件：

- `MDK/output/Project.hex`：调试器下载到 App 区使用。
- `MDK/output/App.bin`：OTA 推荐发送文件。

`App.bin` 大小不能超过 `0x00020000` 字节。

## 开发提示

- 修改分区时，必须同步 BootLoader 工程和 App 工程的 `common/` 文件、App scatter、BootLoader scatter、`SCB->VTOR` 和 PC 脚本目标地址。
- 下载 App hex 时不要整片擦除。
- OTA 串口相关引脚和 DMA 配置集中在 `Components/bsp/mcu_cmic_gd32f470vet6.h`。
