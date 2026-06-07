# GD32F470 V2 BootLoader Project

这是 V2 硬件的 BootLoader 工程，使用 AC6 + CMSIS6。它和同级 App 工程配套，用于实现固件暂存区域（历史兼容名APP2）到 App 运行区（历史兼容名APP1）的可靠升级。

## 工程信息

| 项 | 内容 |
| --- | --- |
| 入口工程 | `MDK/Project.uvprojx` |
| Scatter | `MDK/BootLoader_F470.sct` |
| IROM | `0x08000000 0x00010000` |
| 共用分区 | `common/bl_partition.h` |
| 参数页 | `common/bl_param.h` |
| 核心逻辑 | `BOOTLOADER/Src/bl_core.c` |

## 目录说明

| 目录 | 内容 |
| --- | --- |
| `BOOTLOADER/` | BootLoader 状态机、Flash 擦写接口和头文件。 |
| `common/` | BootLoader / App 共用分区和参数页定义。 |
| `Components/bsp/` | V2 BootLoader 板级初始化，主要用于 USART0 debug 和 OLED I2C0。 |
| `Components/oled/` | BootLoader OLED 驱动，用于显示队伍编号和 Bootloader 状态。 |
| `APP/` | 保留必要的串口、调度等支持代码。 |
| `Driver/`、`Libraries/` | CMSIS6、GD32F4xx 标准外设库和启动文件。 |
| `PACK/` | 工程内置扩展包，例如 perf_counter 2.5.4。 |
| `USER/` | BootLoader 入口 `main.c` 和中断文件。 |
| `MDK/` | Keil 工程、scatter 和输出目录。 |

## 启动流程

1. 初始化 SysTick、debug 串口和 OLED。
2. OLED 显示第1行 `2026055095`、第2行 `Bootloader`。
3. 读取参数页 main copy 和 backup copy。
4. 校验 magic、版本、App 地址、App 大小、CRC 和 tail magic。
5. 参数页异常时，用有效副本或默认参数修复，并写入日志。
6. 如果 `update_flag == BL_UPDATE_FLAG_PENDING`，校验固件暂存区域（历史兼容名APP2）的向量表和 CRC。
7. 校验通过后按 `app_size` 擦写 App 区（历史兼容名APP1），并再次校验 App 区 CRC。
8. 成功后清除升级标志、增加 `update_counter`、写成功日志并复位。
9. 无待升级任务时，校验 App 区向量表并跳转。
10. App 区无效时记录 `BL_ERR_APP1_INVALID` 并停留在 BootLoader。

## Flash 分区

| 区域 | 地址范围 | 大小 |
| --- | --- | --- |
| BootLoader | `0x08000000` - `0x0800FFFF` | 64 KB |
| 参数页 | `0x08010000` - `0x08010FFF` | 4 KB |
| App | `0x08011000` - `0x08030FFF` | 128 KB |
| App备份 | `0x08031000` - `0x08050FFF` | 128 KB |
| 固件暂存 | `0x08051000` - `0x08070FFF` | 128 KB |
| DATA | `0x0807D000` - `0x0807FFFF` | 12 KB |

## 编译和烧录

1. 打开 `MDK/Project.uvprojx`。
2. 确认 Linker 使用 `.\BootLoader_F470.sct`。
3. 编译后将 hex 下载到 `0x08000000`。
4. 再编译并烧录配套 App 工程到 App 区 `0x08011000`，或通过 OTA 写入。

烧录 App 时不要整片擦除，避免清掉 BootLoader 和参数页。
