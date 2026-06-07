# V2BL 代码骨架接口清单

## 全局宏

### 分区相关 (bl_partition.h)
```c
BL_FLASH_BASE_ADDR    0x08000000
BL_FLASH_TOTAL_SIZE   0x00080000  // 512KB
BL_FLASH_PAGE_SIZE    0x00001000  // 4KB
BL_BOOT_START_ADDR          0x08000000  // 64KB
BL_PARAM_START_ADDR         0x08010000  // 4KB
BL_APP1_START_ADDR          0x08011000  // 128KB, App区
BL_APP_BACKUP_START_ADDR    0x08031000  // 128KB, App备份区
BL_APP2_START_ADDR          0x08051000  // 128KB, 固件暂存区
BL_DATA_START_ADDR          0x0807D000  // 12KB
```

### OTA协议 (ota_protocol.h)
```c
OTA_FRAME_MAGIC       0x5AA5C33C
OTA_FRAME_MAX_PAYLOAD 512
OTA_FRAME_TYPE_START  0x01
OTA_FRAME_TYPE_DATA   0x02
OTA_FRAME_TYPE_END    0x03
OTA_FRAME_TYPE_ACK    0x81
OTA_FRAME_TYPE_NACK   0x82
```

### OTA NACK原因码
```c
OTA_NACK_BAD_STATE    1
OTA_NACK_BAD_CRC      2
OTA_NACK_BAD_SEQ      3
OTA_NACK_BAD_OFFSET   4
OTA_NACK_BAD_LENGTH   5
OTA_NACK_FLASH        6
OTA_NACK_FINAL        7
```

### 参数页 (bl_param.h)
```c
BL_PARAM_MAGIC        0x5AA5C33C
BL_PARAM_TAIL_MAGIC   0xA5A5C3C3
BL_PARAM_VERSION      0x00010002
BL_UPDATE_FLAG_IDLE        0x00000000
BL_UPDATE_FLAG_PENDING     0xAA55AA55
BL_UPDATE_FLAG_FAILED      0xDEAD0001
BL_COPY_CHUNK_SIZE         256
```

## 全局变量

### BSP层 (mcu_cmic_gd32f470vet6.c)
```c
__IO uint8_t  oled_cmd_buf[2]        // OLED命令缓冲
__IO uint8_t  oled_data_buf[2]       // OLED数据缓冲
uint8_t  spi3_send_array[ARRAYSIZE]  // SPI3 DMA发送
uint8_t  spi3_receive_array[ARRAYSIZE]
uint8_t  spi1_send_array[ARRAYSIZE]  // SPI0 DMA发送
uint8_t  spi1_receive_array[ARRAYSIZE]
uint8_t  rxbuffer[2048]              // OTA UART DMA
uint8_t  debug_rxbuffer[512]         // Debug UART DMA
uint8_t  usart1_rxbuffer[256]
uint8_t  usart5_rxbuffer[256]
uint16_t adc_value[2]                // ADC DMA采样
uint16_t convertarr[1]               // DAC输出
rtc_parameter_struct rtc_initpara    // RTC参数
```

### APP层
```c
// usart_app.c
__IO uint16_t tx_count
__IO uint8_t  rx_flag
__IO uint16_t uart_dma_len
uint8_t  uart_dma_buffer[2048]
__IO uint8_t  debug_rx_flag
__IO uint16_t debug_uart_dma_len
uint8_t  debug_uart_dma_buffer[512]

// led_app.c
uint8_t ucLed[6] = {1,1,1,1,1,1}    // LED状态数组

// scheduler.c
uint8_t task_num                      // 任务数量
```

## 结构体

### bl_param_t (参数页主结构)
```c
typedef struct {
    uint32_t magic;           // 0x5AA5C33C
    uint32_t version;         // 0x00010002
    uint32_t update_flag;     // IDLE/PENDING/FAILED
    uint32_t app_size;
    uint32_t app_crc32;
    uint32_t app1_addr;
    uint32_t app2_addr;
    uint32_t update_counter;
    uint32_t fail_counter;
    uint32_t last_error;
    uint32_t log_write_index;
    uint32_t reserved[51];
    uint32_t param_crc32;     // CRC覆盖此字段之前
    uint32_t tail_magic;      // 0xA5A5C3C3
} bl_param_t;
```

### bl_log_entry_t (日志条目)
```c
typedef struct {
    uint32_t magic;    // 0xB100B100
    uint32_t seq;
    uint32_t event_id; // 1=PARAM_RECOVER 2=UPDATE_OK 3=UPDATE_FAIL 4=JUMP_FAIL
    uint32_t result;
    uint32_t value0;
    uint32_t value1;
    uint32_t value2;
    uint32_t crc32;
} bl_log_entry_t;
```

### ota_frame_header_t (OTA帧头)
```c
typedef struct {
    uint32_t magic;
    uint8_t  type;
    uint8_t  status;
    uint16_t seq;
    uint32_t offset;
    uint16_t length;
    uint16_t reserved;
    uint32_t crc32;   // 仅覆盖payload
} __attribute__((packed)) ota_frame_header_t;
```

### ota_header_t (V1 START载荷)
```c
typedef struct {
    uint32_t magic;
    uint32_t app_version;
    uint32_t app_size;
    uint32_t app_crc32;
} __attribute__((packed)) ota_header_t;
```

### ota_image_header_v2_t (V2 START载荷)
```c
typedef struct {
    uint32_t magic;
    uint16_t header_version;  // 2
    uint16_t header_size;
    uint32_t app_version;
    uint32_t app_size;
    uint32_t app_crc32;
    uint32_t target_addr;     // 必须=BL_APP1_START_ADDR
    uint32_t image_type;      // 1=APP1
    uint32_t hw_id;
    uint32_t header_crc32;
} __attribute__((packed)) ota_image_header_v2_t;
```

### ota_context_t (OTA会话状态, ota_uart.c 内部)
```c
typedef struct {
    ota_state_t state;         // WAIT_HEADER / RECV_PAYLOAD
    uint32_t app_version;
    uint32_t expected_size;
    uint32_t expected_crc32;
    uint32_t recv_size;
    uint32_t crc_acc;
    uint16_t expected_seq;
    bool     flow_paused;
} ota_context_t;
```

### task_t (调度器任务, scheduler.c 内部)
```c
typedef struct {
    void (*task_func)(void);
    uint32_t rate_ms;
    uint32_t last_run;
} task_t;
```

### ring_buffer_t (环形缓冲)
```c
typedef struct {
    uint8_t *buffer;
    uint32_t size;
    uint32_t head;
    uint32_t tail;
} ring_buffer_t;
```

## 函数清单

### BSP层 (mcu_cmic_gd32f470vet6.c/h)
```c
void     bsp_led_init(void)                    // LED GPIO初始化
void     bsp_btn_init(void)                    // 按键GPIO初始化
void     bsp_oled_init(void)                   // I2C0+DMA初始化
void     bsp_gd25qxx_init(void)                // SPI0+Flash初始化
void     bsp_gd30ad3344_init(void)             // SPI3+ADC芯片初始化
void     bsp_usart0_init(void)                 // Debug串口(115200)
void     bsp_usart1_init(void)                 // 串口1 RS485(115200)+方向控制PE8
void     bsp_usart2_init(void)                 // OTA串口(921600)
void     bsp_usart5_init(void)                 // 串口5(115200)
void     bsp_usart_init(void)                  // 初始化USART0+USART2
void     bsp_usart_all_init(void)              // 初始化全部4路串口
void     bsp_ota_uart_dma_rearm(void)          // OTA DMA重新装载
uint32_t bsp_ota_uart_dma_received_len(void)   // OTA DMA已收字节数
void     bsp_adc_init(void)                    // ADC0+DMA初始化
void     bsp_dac_init(void)                    // DAC0+TIMER5初始化
int      bsp_rtc_init(void)                    // RTC初始化
void     bsp_eth_init(void)                    // ETH RMII GPIO初始化
void     bsp_sdio_init(void)                   // SDIO GPIO初始化
```

### 调度器 (scheduler.c/h)
```c
void scheduler_init(void)     // 从静态表获取任务数
void scheduler_run(void)      // 遍历执行到期任务
```

### 串口应用 (usart_app.c/h)
```c
int  my_printf(uint32_t usart_periph, const char *format, ...)  // 格式化串口输出，RS485方向控制
void uart_task(void)                    // 调度器任务：debug帧回调+USART1帧回调+OTA轮询+采样处理
void ota_reset_state(void)              // 重置OTA解析状态
void debug_uart_frame_callback(const uint8_t *data, uint16_t len)  // Debug帧回调，解析test/conf/start/stop命令
void usart1_frame_callback(const uint8_t *data, uint16_t len)  // USART1帧回调，解析test/conf/start/stop命令
// 内部: static void system_selftest(void)  // 系统自检：Flash/TF卡/RTC，输出到USART1
// 内部: static void config_manage(void)  // 配置管理：从TF卡读取config.ini，更新到Flash
// 内部: static bool config_read(config_data_t *cfg)  // 从Flash读取配置
// 内部: static bool config_write(const config_data_t *cfg)  // 写入配置到Flash
// 内部: static bool parse_ini_value(const char *content, const char *key, uint32_t *value)  // 解析INI值
// 内部: static void sampling_start(void)  // 启动周期采样
// 内部: static void sampling_stop(void)  // 停止周期采样
// 内部: static void sampling_process(void)  // 周期采样处理：LED闪烁+ADC采样+串口输出，OLED状态由oled_task统一显示
// 内部: static float adc_to_voltage(uint16_t adc_val)  // ADC值转电压
```

### LED应用 (led_app.c/h)
```c
void led_task(void)           // 刷新LED状态数组到硬件
// 内部: void led_disp(uint8_t *ucLed)
```

### ADC应用 (adc_app.c/h)
```c
void adc_task(void)           // 更新convertarr[0] = adc_value[0]
```

### OLED应用 (oled_app.c/h)
```c
int  oled_printf(uint8_t x, uint8_t y, const char *format, ...)  // OLED格式化显示
void oled_task(void)          // 固定双行显示：队伍编号2026055095 + IDLE/AutoSample状态
```

### 按键应用 (btn_app.c/h)
```c
void app_btn_init(void)       // 初始化ebtn库(7按键)
void btn_task(void)           // 调用ebtn_process()
// 内部: uint8_t prv_btn_get_state(struct ebtn_btn *btn)
// 内部: void prv_btn_event(struct ebtn_btn *btn, ebtn_evt_t evt)
```

### RTC应用 (rtc_app.c/h)
```c
void rtc_task(void)           // 获取RTC时间缓存，当前不刷新OLED
```

### SD卡应用 (sd_app.c/h)
```c
void sd_fatfs_init(void)      // 使能SDIO中断
void sd_fatfs_test(void)      // SD卡挂载+读写+长文件名测试
// 内部: void card_info_get(void)         // 打印SD卡信息
// 内部: void sd_fatfs_long_name_test(void) // 长文件名测试
```

### OTA串口组件 (ota_uart.c/h)
```c
void ota_uart_reset_state(void)                        // 重置OTA会话+环形缓冲
void ota_uart_process_frame(const uint8_t *data, uint32_t len)  // 原始字节入环形缓冲
void ota_uart_task(void)                               // 从环形缓冲解析帧并执行OTA
// 内部: crc32_update / crc32_finalize / crc32_calc
// 内部: ota_send_ack / ota_send_nack
// 内部: ota_begin — 擦除固件暂存区(历史兼容名APP2),进入RECV_PAYLOAD
// 内部: ota_write_payload — 写flash到固件暂存区(历史兼容名APP2)
// 内部: ota_finalize_if_done — CRC校验+提交参数页
// 内部: flash_erase_pages / flash_program_bytes (.ramfunc段)
// 内内部: param_commit_update — 更新参数页PENDING标志
```

### 环形缓冲 (ring_buffer.h)
```c
void     ring_buffer_init(ring_buffer_t *ring, uint8_t *buffer, uint32_t size)
uint32_t ring_buffer_available(const ring_buffer_t *ring)  // 已用字节数
uint32_t ring_buffer_free(const ring_buffer_t *ring)
uint32_t ring_buffer_free_space(const ring_buffer_t *ring)
bool     ring_buffer_write(ring_buffer_t *ring, const uint8_t *data, uint32_t len)
uint32_t ring_buffer_peek(const ring_buffer_t *ring, uint8_t *data, uint32_t len)
uint32_t ring_buffer_read(ring_buffer_t *ring, uint8_t *data, uint32_t len)
void     ring_buffer_drop(ring_buffer_t *ring, uint32_t len)
void     ring_buffer_clear(ring_buffer_t *ring)
```

### BootLoader核心 (bl_core.c/h)
```c
void bootloader_run(void)     // BL主流程：参数校验→升级/跳转/死循环
void bl_log_dump_uart(void)   // 通过Debug串口打印BL日志
bool bl_commit_param(bl_param_t *param)  // 提交参数（规范化+双副本）
```

### BootLoader Flash接口 (bl_flash_if.c/h)
```c
bool bl_flash_erase(uint32_t addr, uint32_t size)
bool bl_flash_program(uint32_t addr, const uint8_t *data, size_t size)
bool bl_flash_program_param(const void *param, size_t size)  // 擦除+重写参数页
```
