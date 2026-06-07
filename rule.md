# V2BL 硬件+代码规约

## 引脚固定映射（禁止修改）

### LED (GPIOD, Active-High)
| LED | 引脚 |
|-----|------|
| LED1~LED6 | PD10~PD15 |

### 按键 (上拉输入，低有效)
| KEY | 端口 | 引脚 |
|-----|------|------|
| KEY1~KEY5 | GPIOE | PE15/PE6/PE11/PE4/PE7 |
| KEY6 | GPIOB | PB6 |
| KEYW(唤醒) | GPIOA | PA0 |

### 串口
| 用途 | 外设 | TX | RX | 波特率 | DMA |
|------|------|----|----|--------|-----|
| Debug | USART0 | PA9 | PA10 | 115200 | DMA1 CH5 SUB4 |
| 扩展1(RS485) | USART1 | PD5 | PD6 | 115200 | DMA0 CH5 SUB4 |
| OTA | USART2 | PD8 | PD9 | 921600 | DMA0 CH1 SUB4 |
| 扩展5 | USART5 | PC6 | PC7 | 115200 | DMA1 CH1 SUB5 |

### RS485方向控制
| 信号 | 引脚 | 说明 |
|------|------|------|
| RS485_DIR | PE8 | 1=发送模式, 0=接收模式 |

### OLED (I2C0, 400KHz)
| 信号 | 引脚 |
|------|------|
| SCL | PB8 |
| SDA | PB9 |

### SPI Flash (SPI0)
| 信号 | 引脚 |
|------|------|
| SCK | PB3 |
| MISO | PB4 |
| MOSI | PB5 |
| CS | PA15 |

### SPI ADC - GD30AD3344 (SPI3)
| 信号 | 引脚 |
|------|------|
| SCK | PE12 |
| MISO | PE13 |
| MOSI | PE14 |
| CS | PE10 |

### SDIO (4-bit)
| 信号 | 引脚 |
|------|------|
| CLK | PC12 |
| CMD | PD2 |
| DAT0~DAT3 | PC8~PC11 |
| CD(检测) | PE2 |

### ADC
| 通道 | 引脚 |
|------|------|
| CH10 | PC0 |
| CH12(参考) | PC2 |

### DAC
| 通道 | 引脚 |
|------|------|
| DAC0_OUT | PA4 |

### ETH (RMII)
| 信号 | 引脚 |
|------|------|
| MDC/REF_CLK/MDIO/CRS_DV | PC1/PA1/PA2/PA7 |
| RXD0/RXD1 | PC4/PC5 |
| TX_EN/TXD0/TXD1 | PB11/PB12/PB13 |
| PHY_RST | PD3 |

## 通信协议规约

### 通信模式（硬性要求）
- 所有协议帧先按十六进制结构组帧，再以组帧后的 ASCII 字符串形式在串口收发，禁止通过原始 Hex 字节发送。
- 示例：帧头 `0xA5B6` 应发送字符 `A5B6`，即字节 `0x41 0x35 0x42 0x36`，不能发送原始字节 `0xA5 0xB6`。
- 部分功能（告警查询、Bootloader 提示、睡眠唤醒）允许使用纯字符串直接回复，不经协议帧封装，具体以通讯协议细节为准。

### OTA帧格式 (ota_protocol.h)
- 帧头固定20字节：magic(4)+type(1)+status(1)+seq(2)+offset(4)+length(2)+reserved(2)+crc32(4)
- magic = `0x5AA5C33C`
- type: START=0x01, DATA=0x02, END=0x03, ACK=0x81, NACK=0x82
- DATA帧最大payload = 512字节
- crc32仅覆盖payload，不覆盖帧头
- 流控：环形缓冲80%发PAUSE，20%发RESUME

### 参数页规约 (bl_param.h)
- 主参数地址 `0x08010000`，副本 `0x08010100`，日志区 `0x08010200`
- magic=`0x5AA5C33C`, tail_magic=`0xA5A5C3C3`, version=`0x00010002`
- CRC32覆盖param_crc32之前的所有字段
- update_flag: IDLE=0x00, PENDING=0xAA55AA55, FAILED=0xDEAD0001

## 数据通道定义（硬性要求）

| 通道 | 物理来源 | 说明 |
|------|----------|------|
| CH0 | 电位器 | 模拟量采样，经变比换算后上报 |
| CH1 | DAC 回读 | 反映 DAC 输出值，经变比换算后上报 |
| CH2 | 外部 ADC 通道 | PT100（测试板）经过换算后的实际温度值 |

- 无论是单次查询还是自动上报，返回的通道数据必须是 `原始采样值 × 当前变比` 之后的结果。
- 后续采样、查询、自动上报、阈值/告警判断如涉及通道数据，均需优先对齐本通道定义。

## 参数持久化要求（硬性要求）

以下参数写入后必须保存至内部或外部 Flash，设备重启后仍需有效：

1. CH0/CH1 变比
2. CH0/CH1 阈值
3. 设备 ID
4. 波特率配置
5. 告警记录

- 后续涉及上述参数的设置、修改、查询、告警写入逻辑，必须具备掉电/重启保持能力。
- 如使用内部 Flash，必须避开 BootLoader、参数页、App区、App备份区、固件暂存区域等固定分区，禁止破坏 OTA 分区布局。
- 如使用外部 Flash，需复用现有 SPI Flash/GD25QXX 能力，不擅自引入新的存储介质假设。

## MCU内部Flash空间划分（硬性要求）

| 区域 | 起始地址 | 终止地址 | 区域大小 | 说明 |
|------|----------|----------|----------|------|
| Bootloader区 | `0x08000000` | `0x0800FFFF` | 64K | BL本体 |
| 参数区 | `0x08010000` | `0x08010FFF` | 4K | 自行选放参数，升级固件不调用 |
| App区 | `0x08011000` | `0x08030FFF` | 128K | 当前运行App（历史兼容名APP1） |
| App备份区 | `0x08031000` | `0x08050FFF` | 128K | 预留备份区，当前未接入回滚 |
| 固件暂存区域 | `0x08051000` | `0x08070FFF` | 128K | OTA下载暂存区（历史兼容名APP2） |
| DATA | `0x0807D000` | `0x0807FFFF` | 12K | 预留用户数据，位于照片未占用尾部空间 |

- `BL_APP2_*` 为历史兼容命名，实际表示固件暂存区域，不是App备份区。
- BootLoader、App、Python脚本中的APP起始地址必须一致为 `0x08011000`。
- APP.bin 不超过 128KB (`0x20000` 字节)。

## OLED显示要求（硬性要求）

系统运行过程中，OLED 始终以双行文本显示：

| 行号 | 内容 |
|------|------|
| 第1行 | 队伍编号 `2026055095` |
| 第2行 | 状态信息 |

状态信息规则：

- 工作在 Bootloader 区域时，第2行显示 `Bootloader`。
- 工作在 APP 区域且处于自动采集上报过程时，第2行显示 `AutoSample`。
- APP 其余时刻，第2行显示 `IDLE`。
- APP 不再将按键、tick、ADC、RTC 时间等调试信息刷新到 OLED，采样数据仍通过串口上报。

## 编码规范
- 全部使用C99，`<stdint.h>`类型
- 头文件使用 `#ifndef` 守卫
- extern "C" 包裹（兼容C++链接）
- volatile标记硬件寄存器和中断共享变量
- DMA缓冲区不做对齐要求（F470无严格对齐限制）
- flash操作函数必须放在 `.ramfunc` 段（`__attribute__((section(".ramfunc"), noinline))`）
- printf重定向使用 `my_printf(usart_periph, format, ...)` 或 `oled_printf(x, y, format, ...)`

## 底层限制
- APP.bin 不超过 128KB (`0x20000` 字节)
- BootLoader/App/Python脚本中APP起始地址必须一致为 `0x08011000`
- BL和App的 `bl_partition.h`、`bl_param.h` 必须同步
- Flash擦除粒度为4KB页
- OTA ring buffer 16KB，流控阈值80%/20%
- DMA接收缓冲：OTA=2048B, Debug=512B

## 禁止写法
- 禁止修改Flash分区地址定义
- 禁止修改OTA帧协议magic/type定义
- 禁止在flash操作函数中调用非RAM段函数
- 禁止删除参数页双副本机制
- 禁止在BootLoader中使用中断（跳转前全部关闭）
- 禁止使用 `fmc_mass_erase()` （会擦除BL+参数页）
- Keil下载App时禁止整片擦除，必须按扇区擦除
