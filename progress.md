# V2BL 开发进度

## 已完成功能
- [x] BSP层全部外设初始化（LED/KEY/OLED/SPI Flash/SPI ADC/USART/ADC/DAC/RTC/SDIO/ETH）
- [x] 调度器框架（基于时间片的协作式调度）
- [x] LED控制（6路LED，active-high，状态数组驱动）
- [x] 按键驱动（7按键，ebtn库，消抖+点击事件，点击切换LED）
- [x] OLED显示（I2C0 DMA，双行显示队伍编号2026055095和状态Bootloader/IDLE/AutoSample）
- [x] ADC采集（ADC0 CH10+CH12，DMA循环采集）
- [x] DAC输出（TIMER5触发）
- [x] RTC实时时钟（LXTAL时钟源，显示时分秒）
- [x] SD卡FATFS（SDIO 4bit，读写测试+长文件名测试）
- [x] SPI Flash（GD25QXX，SPI0 DMA）
- [x] SPI ADC（GD30AD3344，SPI3）
- [x] UART OTA完整流程（START/DATA/END帧协议，环形缓冲，流控PAUSE/RESUME）
- [x] BootLoader（参数双副本校验，固件暂存区校验+拷贝到App区+跳转，日志系统）
- [x] Debug串口（USART0，115200，DMA接收，空闲中断帧检测）
- [x] OTA串口（USART2，921600，DMA循环接收）
- [x] PC端OTA发送脚本（`Tools/ota_uart_sender.py`）
- [x] PC端OTA发送脚本兼容赛题固件包：自动剥离V1/V2固件前导4字节魔术字后发送裸APP镜像
- [x] 系统自检功能（串口输入test指令，检测Flash/TF卡/RTC，输出自检结果）
- [x] USART1命令解析（空闲中断帧检测，支持test指令）
- [x] RS485方向控制（PE8引脚，my_printf自动切换发送/接收模式）
- [x] 配置管理功能（串口输入conf指令，从TF卡读取config.ini，更新Ratio和Limit到Flash）
- [x] 周期采样功能（start/stop指令控制，LED1闪烁，串口输出采样数据，OLED显示AutoSample/IDLE状态）

## 当前调试问题
- （暂无记录）

## 现存BUG
- （暂无记录）

## 已修复BUG
- [x] RTC时间读取错误：GD32 RTC使用BCD格式，已添加bcd_to_dec转换函数
- [x] RS485发送乱码：添加发送前后延迟，确保RS485芯片方向切换完成
- [x] 编译错误：usart_app.c中缺少adc_value变量的extern声明

## 代码重写记录
- [x] scheduler.c：重写调度器代码，改变变量命名和代码结构
- [x] sd_app.c：重写SD卡模块代码，改变函数命名和实现方式
- [x] ota_uart.c：重写OTA协议代码，改变状态机实现和变量命名

## BL工程降重修改
- [x] BL/APP/scheduler.c：改变量命名、任务表结构体写法、循环方式
- [x] BL/APP/usart_app.c：改缓冲区大小、打印函数命名、uart_task逻辑
- [x] BL/Components/bsp/mcu_cmic_gd32f470vet6.c：改引脚映射写法、初始化顺序、注释
- [x] BL/Components/bsp/mcu_cmic_gd32f470vet6.h：改宏命名风格、注释
- [x] BL/common/bl_param.h：改宏命名风格、注释
- [x] BL/common/bl_partition.h：改宏命名风格、注释
- [x] BL/USER/src/gd32f4xx_it.c：改DMA处理流程、变量命名、缓冲区处理方式

## 待开发任务
- （暂无记录）

## 临时搁置
- 组合按键功能（代码已注释，ebtn_combo相关）
- ETH RMII初始化已编写但未集成到调度器
- RTC备份寄存器检测逻辑已注释（当前每次复位重置时间）
- SD卡任务未加入调度器（仅main中一次性测试）
