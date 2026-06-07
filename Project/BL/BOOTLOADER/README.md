# BootLoader Component

本目录是 GD32F470 V2 BootLoader 的核心组件，配套 `common/` 中的分区和参数页定义使用。

## 文件说明

| 文件 | 作用 |
| --- | --- |
| `Inc/bl_core.h` | BootLoader 入口声明。 |
| `Inc/bl_flash_if.h` | 内部 Flash 擦写接口声明。 |
| `Src/bl_core.c` | 参数页恢复、升级判断、固件暂存区校验、暂存固件到 App 区拷贝、App 区校验和跳转。 |
| `Src/bl_flash_if.c` | Flash 按页擦除、字节编程和参数页提交辅助函数。 |

相关公共头文件：

- `../common/bl_partition.h`：Flash 分区。
- `../common/bl_param.h`：参数页、日志、升级标志和错误码。
- `../USER/src/main.c`：初始化 SysTick / 串口 / OLED 后显示 Bootloader 状态，再调用 `bootloader_run()`。
- `../Components/bsp/mcu_cmic_gd32f470vet6.*`：V2 BootLoader BSP 子集，主要初始化 USART0 debug 和 OLED I2C0。
- `../Components/oled/`：BootLoader OLED 驱动，固定显示队伍编号 `2026055095` 和 `Bootloader` 状态。

## 参数页

| 项 | 地址 / 含义 |
| --- | --- |
| main 参数 | `0x08010000` |
| backup 参数 | `0x08010100` |
| 日志区 | `0x08010200` |
| 日志条目 | 32 条，每条 32 字节 |

App 完成 OTA 下载后需要提交 `bl_param_t`：

- `update_flag = BL_UPDATE_FLAG_PENDING`
- `app_size = downloaded_size`
- `app_crc32 = crc32_of_app2_payload`
- `app1_addr = BL_APP1_START_ADDR`
- `app2_addr = BL_APP2_START_ADDR`
- `param_crc32` 为 `bl_param_t` 中 `param_crc32` 前面字段的 CRC32

随后 App 调用 `NVIC_SystemReset()`，BootLoader 在下一次启动时处理升级。

## 维护要求

- BootLoader 和 App 的 `common/bl_partition.h`、`common/bl_param.h` 必须保持一致。
- BootLoader scatter 不能超过 `0x08000000` - `0x0800FFFF`。
- App 区起始地址 `0x08011000` 必须与 App scatter 和 `SCB->VTOR` 一致。
- 修改升级协议时，同步更新 App 的 `Components/ota_uart/` 和 PC 端 `Tools/ota_uart_sender.py`。
