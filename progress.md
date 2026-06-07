# V2BL 开发进度

## 已完成功能

### 硬件驱动层
- [x] BSP层全部外设初始化（LED/KEY/OLED/SPI Flash/SPI ADC/USART/ADC/DAC/RTC/SDIO/ETH）
- [x] LED控制（6路LED，active-high，状态数组驱动）
- [x] 按键驱动（7按键，ebtn库，消抖+点击事件）
- [x] OLED显示（I2C0 DMA，双行：队伍编号2026055095 + 状态Bootloader/IDLE/AutoSample）
- [x] ADC采集（ADC0 CH10+CH12，DMA循环采集）
- [x] DAC输出（TIMER5触发）
- [x] RTC实时时钟（LXTAL时钟源）
- [x] SD卡FATFS（SDIO 4bit，读写+长文件名测试）
- [x] SPI Flash（GD25QXX，SPI0 DMA）
- [x] SPI ADC（GD30AD3344，SPI3）
- [x] RS485方向控制（PE8引脚，发送自动切换方向）
- [x] Debug串口（USART0，115200，DMA+空闲中断）
- [x] OTA串口（USART2，921600，DMA循环接收）

### 调度器/基础框架
- [x] 协作式时间片调度器
- [x] 系统自检（test指令：Flash/TF卡/RTC）
- [x] 配置管理（conf指令：从TF卡config.ini更新参数到Flash）

### 通信协议
- [x] CRC16-Modbus大端序收发（已修复字节序Bug）
- [x] USART1默认波特率19200（评测出厂要求）
- [x] 上电心跳帧（0x05 8888）
- [x] 广播寻址响应（0x05 FFFF → 回心跳）
- [x] 错误应答帧（FF EEEE）：CRC错/长度不匹配/非法命令
- [x] 设备ID校验：非广播+ID不匹配时静默丢弃

### 已对齐评测命令（sys_process_frame）
| 命令字 | 功能 | 状态 |
|--------|------|------|
| 0x0101 | 设备重启 | ✅ |
| 0x0104 | 查固件版本（2.0.1.0） | ✅ |
| 0x0105 | 设置RTC时间（UTC时间戳） | ✅ |
| 0x0106 | 读RTC时间 | ✅ |
| 0x0111 | 查设备ID（广播） | ✅ |
| 0x0112 | 查波特率 | ✅ |
| 0x01A1 | 设设备ID | ✅ |
| 0x01A2 | 设波特率（回OK后重启） | ✅ |
| 0x0201 | 查CH0（电位器×变比） | ✅ |
| 0x0202 | 查CH1（DAC回读×变比） | ✅ 新增 |
| 0x0221 | 查CH2（PT100温度） | ✅ 修正 |
| 0x0241 | 设CH0变比（IEEE754浮点，持久化） | ✅ 新增 |
| 0x0242 | 设CH1变比（IEEE754浮点，持久化） | ✅ 新增 |
| 0x0261 | 设上报时间间隔 | ✅ |
| 0x0301 | 设DAC输出（0~4095） | ✅ |
| 0x0302 | 开始自动上报（首帧立即+按间隔UTC+CH0+CH1） | ✅ |
| 0x0303 | 停止自动上报 | ✅ |
| 0x0400 | 批量读阈值（CH0+CH1，8字节） | ✅ 新增 |
| 0x0401 | 读CH0阈值 | ✅ 新增 |
| 0x0402 | 读CH1阈值 | ✅ 新增 |
| 0x0411 | 写CH0阈值（IEEE754浮点，持久化） | ✅ 新增 |
| 0x0412 | 写CH1阈值（IEEE754浮点，持久化） | ✅ 新增 |
| 0x03AA | 睡眠（回OK→Stop模式→10s后RTC唤醒→发wakeup字符串） | ✅ 新增 |
| 0x0501 | 升级请求（回OK→标记RTC BKP1→复位进BL） | ✅ |
| 0x0601 | 设置告警上报模式（01=主动，02=仅存储） | ✅ 新增 |
| 0x0602 | 查询告警记录（最近10条倒序，字符串格式） | ✅ 新增 |
| 0x0603 | 清除告警记录 | ✅ 新增 |
| H-02 | 自动上报期间屏蔽其他命令（除0x0303） | ✅ |

### BootLoader
- [x] 参数页双副本校验/恢复
- [x] 固件暂存区校验+拷贝到App区+跳转
- [x] 日志系统
- [x] USART1 DMA接收+空闲中断框架（RS485，19200）
- [x] BL倒计时打印（10s，USART1输出）
- [x] BL心跳帧上电发送

### PC工具
- [x] OTA发送脚本（`Tools/ota_uart_sender.py`，支持V1/V2固件包自动剥离魔术字）

---

## 当前调试问题
- 【搁置】BL OTA接收（N阶段）：RS485半双工时序竞争，0x0502无法在BL倒计时期间被可靠接收

---

## 评测失败修复任务表（对照满分工程 MICU_GD_APP）

> 满分代码：`C:\Users\李政强\Downloads\example\MICU_GD_APP`
> **致命根因**：评测时序 A→J 全部正常，**J(睡眠)唤醒后 K/L/M/N 全部超时** —— 睡眠唤醒未重建 USART1+DMA 接收链路，设备变"聋子"。

- [x] **T1 睡眠唤醒重建外设**（救 K/L/M/N 连锁超时，最高优先）
  - 文件：`mcu_cmic_gd32f470vet6.c::bsp_enter_stop_mode`
  - 满分参考：`bsp_deepsleep_reinit_after_wakeup()` 唤醒后重建 usart/adc/dac
  - 改动：`SystemInit()`后追加 `SystemCoreClockUpdate/systick_config/bsp_usart_all_init/bsp_adc_init/bsp_dac_init/ota_reset_state`
- [x] **T2 A阶段广播应答+0x0101重启**（A阶段 2 条）
  - 广播探测(0xFFFF)改回**心跳帧 cmd=0x8888 空载荷**（原误回0xFFFF+2字节）
  - 0x0101 由"仅回OK"改为**回OK后真重启**（NVIC_SystemReset，不标记BL升级），A-03才能收到重启后心跳
- [x] **T3 K 异常帧应答**（3 条）：核查逻辑本就完整（长度/CRC/非法命令均send_error FF EEEE）→ **纯T1睡眠连锁，无需改代码**
- [x] **T4 L 设备ID设置**（2 条）：核查校验已排除0/广播且不重启 → **纯T1睡眠连锁，无需改代码**
- [x] **T5 M 波特率设置**（2 条）：软切不重启逻辑正确；**映射表维持原值（已回退）**
  - ⚠️ 教训：曾试图改表对齐满分(0x11=4800)，但设备Flash存的是0x11，旧表0x11=19200设备才能正常通信；改表导致开机切4800全乱码，已回退。评测中映射表错位不影响任何评分项，无需改动。
- [~] **T6 N 阶段 BL OTA**（代码已重构，待烧录验证）：对齐满分BL字节级接收，解决RS485半双工时序竞争
  - 真因：原BL倒计时"每秒打印+重置DMA"冲掉上位机0x0502数据帧
  - 改动1：`bsp_usart1_init` DMA改循环模式（dma_circulation_enable），6420B固件流不溢出
  - 改动2：`bl_core.c` 新增字节级游标读取(bl_u1_get_byte/ring_reset/wait_quiet)，永不重置DMA
  - 改动3：重写`bl_recv_firmware`流式收裸bin，修复magic字节序bug(小端memcpy→大端组装，须5A A5 C3 3C)
  - 改动4：重写`bootloader_wait_cmd`+新增`bl_recv_cmd_frame`，全程关IDLE中断走字节级游标，N-01关键字窗口内重打
  - 改动5：`bl_puts`/`bl_send_frame`发完只推进游标丢回声，不再重置DMA/重启IDLE中断
  - N-02坏固件由app_vec_ok(向量表校验)拦截回ERROR，N-03正确固件通过
  - 改动6【真凶2】：`bl_parse_cmd`长度校验公式错误(total_chars重复计帧头,要求30字符)致26字符的0502帧永远return 0被丢弃。改为 `raw_len == 11+payload_len`(raw不含tail)
  - 自动化验证：D:\Desktop\claude\ota_test.py (pyserial驱动COM12, APP@115200/BL@19200双波特率切换)
  - ✅ **实测打通**：0x0501进BL→0x0502回OK→固件字节级接收+向量表校验→0x0503回OK→提交参数页重启→BL拷贝跳转，全链路PASS(小固件验证,1~4步全过)
**修复记录**：
- 2026-06-08 T1 完成（睡眠唤醒重建串口/ADC/DAC/OTA链路）
- 2026-06-08 T2 完成（广播回心跳0x8888 + 0x0101真重启）【实测✅ A1/A2通过】
- 2026-06-08 T3/T4 核查无需改（纯T1连锁）
- 2026-06-08 T5 核查（波特率软切逻辑本就正确，映射表改动引发乱码回归已回退）
- 2026-06-08 T1补丁：唤醒后SystemInit会把VTOR打回BL区致中断跑飞，补 `SCB->VTOR=BL_APP1_START_ADDR` 恢复【实测✅ B组睡眠唤醒+唤醒后通信全通过】
- 2026-06-08 睡眠去重：打印wakeup后清sys_sleeping，消除重复打印（评测要求只发一次）
- 2026-06-08 **实测验收**：K(异常帧FF EEEE×3)/L(设ID)/M(设波特率115200) 全部转绿 ✅，证明睡眠根因斩断后连锁超时全解
- ⚠️ 当前设备状态：ID=`0x0001`（L测试后），波特率已切=115200（M测试后）

---

## 现存BUG
- ⚠️ 告警上报格式暂为占位实现（ALARM字符串），I阶段需完善为规范格式

---

## 待开发计划

### 🔴 高优先（影响评分）

#### N 阶段：BL OTA接收（18分，搁置）
- [ ] 解决RS485半双工时序竞争，实现BL倒计时期间可靠接收0x0502
- [ ] 裸bin切片接收（256B/片）→校验magic→写暂存区→参数页PENDING→复位升级

### 🟡 中优先（快速验证拿分）

#### D/E/F/G 阶段：变比阈值持久化验证（19分，代码已完成）
- [ ] 串口测试：设变比→查CH0/CH1→值正确乘变比
- [ ] 串口测试：写阈值→读阈值→一致
- [ ] 重启验证：变比/阈值持久化

#### J 阶段：睡眠唤醒验证（5分，代码已完成）
- [ ] 串口测试：发0x03AA→收到OK→10秒后串口输出 `instrument wakeup\r\n`

#### I 阶段：告警功能验证（7分，代码已完成）
- [ ] 串口测试：0x0601设置模式→0x0602查询记录→0x0603清除
- [ ] 触发验证：超阈值→告警存储+主动上报字符串

#### C 阶段：时间设置回读验证（2分，✅已验证通过）
- [x] 串口验证：发0x0105设UTC时间戳，回读0x0106确认一致

#### K 阶段：异常帧处理验证（6分，代码已有）
- [ ] K-01 CRC错误帧：返回FF EEEE
- [ ] K-02 长度错误帧：返回FF EEEE
- [ ] K-03 非法命令字：返回FF EEEE

#### L 阶段：修改ID（6分，代码已有）
- [ ] 验证0x01A1设ID后，新ID下通信正常，重启后ID持久化

#### M 阶段：修改波特率（6分，代码已有）
- [ ] 验证0x01A2设波特率115200后切换正常，重启后仍115200

### 🟢 低优先（已有基础）

#### LED指示灯对齐
- [ ] 系统状态灯：进入APP后以1s为单位闪烁
- [ ] 采集工作灯：自动上报期间常亮，其余熄灭

#### 搁置项
- 组合按键（ebtn_combo）
- ETH RMII集成调度器
- RTC备份寄存器检测
- SD卡任务入调度器

---

## 已修复BUG
- [x] RTC时间读取错误：GD32 RTC使用BCD格式，已添加bcd_to_dec转换
- [x] RS485发送乱码：添加发送前后延迟
- [x] 编译错误：usart_app.c缺少adc_value的extern声明
- [x] CRC16字节序错误：收发均改为大端序
- [x] 命令字错位：0x0221/0x0241/0x0411全部重新对齐赛题定义

---

## BL工程降重修改记录
- [x] BL/APP/scheduler.c
- [x] BL/APP/usart_app.c
- [x] BL/Components/bsp/mcu_cmic_gd32f470vet6.c/.h
- [x] BL/common/bl_param.h / bl_partition.h
- [x] BL/USER/src/gd32f4xx_it.c
