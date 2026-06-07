/*
 * 串口任务
 * 处理debug串口帧回调 + ota dma轮询
 * 2026-05 改写
 */
#include "mcu_cmic_gd32f470vet6.h"
#include "usart_app.h"
#include "ota_uart.h"
#include "rtc_app.h"
#include "adc_app.h"
#include "gd30ad3344.h"
#include "bl_partition.h"

extern uint8_t rxbuffer[OTA_UART_RXBUF_SIZE];
extern uint8_t debug_rxbuffer[DEBUG_UART_RXBUF_SIZE];
extern uint8_t usart1_rxbuffer[256];
extern rtc_parameter_struct rtc_initpara;
extern uint16_t adc_value[2];
extern uint16_t convertarr[CONVERT_NUM];

/* 配置数据存储在DATA区域 */
#define CONFIG_DATA_ADDR  BL_DATA_START_ADDR
#define CONFIG_MAGIC      0x434F4E46UL  /* "CONF" */
#define CONFIG_DEFAULT_RATIO      1UL
#define CONFIG_DEFAULT_LIMIT      5UL
#define CONFIG_DEFAULT_THRESHOLD  2500UL
#define CONFIG_DEFAULT_PERIOD     5UL
#define CONFIG_DEFAULT_DAC        0UL
#define CONFIG_DEFAULT_BAUD_CODE  0x13U
#define CONFIG_DEFAULT_RATIO_F    1.0f       /* 默认变比1.0 */
#define CONFIG_DEFAULT_THR_F      2.5f       /* 默认阈值2.5 */

#define DEVICE_ID_DEFAULT    0x0001U
#define DEVICE_ID_MIN        0x0001U
#define DEVICE_ID_MAX        0xFFFEU
#define DEVICE_ID_BROADCAST  0xFFFFU

#define FW_VER_MAJOR         0x02U
#define FW_VER_MINOR         0x00U
#define FW_VER_REVISION      0x01U
#define FW_VER_BUILD         0x00U

#define SYS_HEAD_HI          0xA5U
#define SYS_HEAD_LO          0xB6U
#define SYS_TAIL_HI          0xB6U
#define SYS_TAIL_LO          0xA5U
#define SYS_FRAME_FIXED      0x02U
#define SYS_FRAME_MIN_LEN    13U
#define SYS_TYPE_REQ         0x01U
#define SYS_TYPE_RSP         0x02U
#define SYS_TYPE_RESET       0x05U
#define SYS_TYPE_ERR         0xFFU
#define SYS_CMD_HANDSHAKE    0x0101U
#define SYS_CMD_VER_QUERY    0x0104U
#define SYS_CMD_TIME_SET     0x0105U
#define SYS_CMD_TIME_QUERY   0x0106U
#define SYS_CMD_ID_QUERY     0x0111U
#define SYS_CMD_BAUD_QUERY   0x0112U
#define SYS_CMD_ID_SET       0x01A1U
#define SYS_CMD_BAUD_SET     0x01A2U
#define SYS_CMD_CH0_QUERY    0x0201U
#define SYS_CMD_CH1_QUERY    0x0202U
#define SYS_CMD_CH2_QUERY    0x0221U
#define SYS_CMD_TEMP_OR_LIMIT 0x0241U
#define SYS_CMD_CH1_RATIO_SET 0x0242U
#define SYS_CMD_THR_QUERY_ALL 0x0400U  /* 批量读阈值(CH0+CH1) */
#define SYS_CMD_CH0_THR_QUERY 0x0401U
#define SYS_CMD_CH1_THR_QUERY 0x0402U
#define SYS_CMD_CH0_THR_SET   0x0411U
#define SYS_CMD_CH1_THR_SET   0x0412U
#define SYS_CMD_PERIOD_SET   0x0261U
#define SYS_CMD_DAC_SET      0x0301U
#define SYS_CMD_AUTO_REPORT  0x0302U
#define SYS_CMD_AUTO_STOP    0x0303U
#define SYS_CMD_SLEEP        0x03AAU
#define SYS_CMD_ALARM_SET    0x0601U  /* 设置是否主动上报告警 */
#define SYS_CMD_ALARM_QUERY  0x0602U  /* 查询告警记录 */
#define SYS_CMD_ALARM_CLEAR  0x0603U  /* 清除告警记录 */
#define SYS_CMD_BOOT_ENTER   0x0501U
#define SYS_CMD_OTA_DATA     0x0502U
#define SYS_CMD_OTA_DONE     0x0503U
#define SYS_CMD_RESET        0x8888U
#define SYS_CMD_ERR          0xEEEEU

/* App请求BootLoader等待升级，使用RTC备份寄存器跨软件复位传递 */
#define BOOT_REQ_MAGIC          0xA55A5AA5UL

typedef struct {
    uint32_t magic;
    uint32_t ratio;
    uint32_t limit;
    uint32_t crc32;
} config_legacy_data_t;

typedef struct {
    uint32_t magic;
    uint32_t ratio;
    uint32_t limit;
    uint32_t device_id;
    uint32_t crc32;
} config_v1_data_t;

typedef struct {
    uint32_t magic;          /* CONFIG_MAGIC */
    uint32_t ratio;          /* 兼容旧字段：保留 */
    uint32_t limit;          /* 兼容旧配置：阈值/采样周期 */
    uint32_t device_id;      /* 设备ID: 0x0001~0xFFFE */
    uint32_t ch0_limit_mv;   /* 兼容旧字段：保留 */
    uint32_t ch1_limit_mv;   /* 兼容旧字段：保留 */
    uint32_t ch0_period_sec; /* CH0采集周期，单位秒 */
    uint32_t ch1_period_sec; /* CH1采集周期，单位秒 */
    uint32_t dac_value;      /* DAC输出值：0~4095 */
    uint32_t baud_code;      /* 0x11/0x12/0x13/0x14 */
    uint32_t ch0_ratio_bits; /* CH0变比，IEEE754位模式 */
    uint32_t ch1_ratio_bits; /* CH1变比，IEEE754位模式 */
    uint32_t ch0_thr_bits;   /* CH0阈值，IEEE754位模式 */
    uint32_t ch1_thr_bits;   /* CH1阈值，IEEE754位模式 */
    uint32_t crc32;          /* CRC32校验 */
} config_data_t;

/* 文件系统变量 */
static FATFS config_fs;

/* 周期采样相关变量 */
static volatile uint8_t sampling_active = 0;      /* 采样状态：0=停止，1=运行 */
static volatile uint32_t sampling_cycle_ms = 5000; /* 采样周期，默认5秒 */
static volatile uint32_t sampling_last_time = 0;   /* 上次采样时间 */
static volatile uint8_t sys_sleeping = 0;           /* 协议睡眠状态 */
volatile uint8_t rtc_alarm_wakeup_flag = 0;         /* RTC闹钟唤醒标志 */
static uint8_t alarm_state = 0;                     /* bit0=CH0告警, bit1=CH1告警 */

/* 告警记录结构（16字节） */
typedef struct {
    uint32_t timestamp;   /* UTC时间戳 */
    uint8_t  channel;     /* 0=CH0, 1=CH1 */
    uint8_t  reserved[3]; /* 对齐 */
    float    threshold;   /* 阈值 */
    float    actual;      /* 实际值 */
} alarm_record_t;

#define ALARM_RECORD_MAX   64           /* 最多存储64条 */
#define ALARM_FLASH_ADDR   0x00000000   /* 告警记录在外部Flash的起始地址 */
#define ALARM_MODE_REPORT  0x01         /* 主动上报 */
#define ALARM_MODE_STORE   0x02         /* 仅存储 */

static uint8_t alarm_mode = ALARM_MODE_REPORT;     /* 默认主动上报 */
static uint16_t alarm_wr_index = 0;                /* 写索引（循环） */
static uint16_t alarm_count = 0;                   /* 当前记录数 */


__IO uint8_t  debug_rx_flag = 0;
__IO uint16_t debug_uart_dma_len = 0;
uint8_t debug_uart_dma_buffer[DEBUG_UART_RXBUF_SIZE];

__IO uint8_t  usart1_rx_flag = 0;
__IO uint16_t usart1_uart_dma_len = 0;
uint8_t usart1_uart_dma_buffer[256];

/* ota dma游标，记录上次处理到哪 */
static uint32_t ota_pos = 0;

int my_printf(uint32_t usart, const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    int n, i;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    /* RS485方向控制：USART1发送前设为发送模式 */
    if (usart == USART1) {
        gpio_bit_set(GPIOE, GPIO_PIN_8);  /* RS485_DIR = 1, 发送模式 */
        delay_us(50);  /* 等待RS485芯片切换到发送模式 */
    }

    for(i = 0; i < n; i++){
        usart_data_transmit(usart, buf[i]);
        while(RESET == usart_flag_get(usart, USART_FLAG_TBE));
    }

    /* 等待最后一个字节发送完成 */
    while(RESET == usart_flag_get(usart, USART_FLAG_TC));

    /* RS485方向控制：USART1发送完成后设为接收模式 */
    if (usart == USART1) {
        delay_us(50);  /* 等待最后一个字节完全发送 */
        gpio_bit_reset(GPIOE, GPIO_PIN_8);  /* RS485_DIR = 0, 接收模式 */
    }

    return n;
}

void ota_reset_state(void)
{
    ota_pos = 0;
    ota_reset();
}

/*
 * ota dma轮询
 * 循环dma，用 NDTR 算写位置，处理回绕
 */
static void ota_dma_poll(void)
{
    uint32_t pos, tail;

    pos = OTA_UART_RXBUF_SIZE - dma_transfer_number_get(OTA_UART_DMA, OTA_UART_DMA_CH);
    if(pos == ota_pos) return;

    if(pos > ota_pos){
        ota_feed(&rxbuffer[ota_pos], pos - ota_pos);
    } else {
        tail = OTA_UART_RXBUF_SIZE - ota_pos;
        ota_feed(&rxbuffer[ota_pos], tail);
        if(pos > 0) ota_feed(rxbuffer, pos);
    }
    ota_pos = pos;
}

/* BCD转十进制 */
static uint8_t bcd_to_dec(uint8_t bcd)
{
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

/* 简单的CRC32计算 */
static uint32_t calc_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    uint32_t i, j;

    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFF;
}

/* float与IEEE754位模式互转（定义见后） */
static float bits_to_float(uint32_t bits);
static uint32_t float_to_bits(float f);

/* 从Flash读取配置 */
static void config_default(config_data_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->magic = CONFIG_MAGIC;
    cfg->ratio = CONFIG_DEFAULT_RATIO;
    cfg->limit = CONFIG_DEFAULT_LIMIT;
    cfg->device_id = DEVICE_ID_DEFAULT;
    cfg->ch0_limit_mv = CONFIG_DEFAULT_THRESHOLD;
    cfg->ch1_limit_mv = CONFIG_DEFAULT_THRESHOLD;
    cfg->ch0_period_sec = CONFIG_DEFAULT_PERIOD;
    cfg->ch1_period_sec = CONFIG_DEFAULT_PERIOD;
    cfg->dac_value = CONFIG_DEFAULT_DAC;
    cfg->baud_code = CONFIG_DEFAULT_BAUD_CODE;
    cfg->ch0_ratio_bits = float_to_bits(CONFIG_DEFAULT_RATIO_F);
    cfg->ch1_ratio_bits = float_to_bits(CONFIG_DEFAULT_RATIO_F);
    cfg->ch0_thr_bits = float_to_bits(CONFIG_DEFAULT_THR_F);
    cfg->ch1_thr_bits = float_to_bits(CONFIG_DEFAULT_THR_F);
}

static bool device_id_valid(uint32_t id)
{
    return id >= DEVICE_ID_MIN && id <= DEVICE_ID_MAX;
}

static bool config_read(config_data_t *cfg)
{
    const config_data_t *flash_cfg = (const config_data_t *)CONFIG_DATA_ADDR;
    const config_v1_data_t *v1_cfg = (const config_v1_data_t *)CONFIG_DATA_ADDR;
    const config_legacy_data_t *legacy_cfg = (const config_legacy_data_t *)CONFIG_DATA_ADDR;
    uint32_t crc;

    if (flash_cfg->magic != CONFIG_MAGIC) {
        return false;
    }

    crc = calc_crc32((const uint8_t *)flash_cfg, offsetof(config_data_t, crc32));
    if (crc == flash_cfg->crc32 && device_id_valid(flash_cfg->device_id)) {
        *cfg = *flash_cfg;
        return true;
    }

    crc = calc_crc32((const uint8_t *)v1_cfg, offsetof(config_v1_data_t, crc32));
    if (crc == v1_cfg->crc32 && device_id_valid(v1_cfg->device_id)) {
        config_default(cfg);
        cfg->ratio = v1_cfg->ratio;
        cfg->limit = v1_cfg->limit;
        cfg->device_id = v1_cfg->device_id;
        return true;
    }

    crc = calc_crc32((const uint8_t *)legacy_cfg, offsetof(config_legacy_data_t, crc32));
    if (crc == legacy_cfg->crc32) {
        config_default(cfg);
        cfg->ratio = legacy_cfg->ratio;
        cfg->limit = legacy_cfg->limit;
        return true;
    }

    return false;
}

/* 写入配置到Flash */
static bool config_write(const config_data_t *cfg)
{
    config_data_t cfg_with_crc = *cfg;

    cfg_with_crc.magic = CONFIG_MAGIC;
    if (!device_id_valid(cfg_with_crc.device_id)) {
        cfg_with_crc.device_id = DEVICE_ID_DEFAULT;
    }
    if (cfg_with_crc.ratio == 0U) {
        cfg_with_crc.ratio = CONFIG_DEFAULT_RATIO;
    }
    if (cfg_with_crc.limit == 0U) {
        cfg_with_crc.limit = CONFIG_DEFAULT_LIMIT;
    }
    if (cfg_with_crc.ch0_limit_mv == 0U) {
        cfg_with_crc.ch0_limit_mv = CONFIG_DEFAULT_THRESHOLD;
    }
    if (cfg_with_crc.ch1_limit_mv == 0U) {
        cfg_with_crc.ch1_limit_mv = CONFIG_DEFAULT_THRESHOLD;
    }
    if (cfg_with_crc.ch0_period_sec == 0U) {
        cfg_with_crc.ch0_period_sec = CONFIG_DEFAULT_PERIOD;
    }
    if (cfg_with_crc.ch1_period_sec == 0U) {
        cfg_with_crc.ch1_period_sec = CONFIG_DEFAULT_PERIOD;
    }
    if (cfg_with_crc.baud_code == 0U) {
        cfg_with_crc.baud_code = CONFIG_DEFAULT_BAUD_CODE;
    }
    if (cfg_with_crc.ch0_ratio_bits == 0U) {
        cfg_with_crc.ch0_ratio_bits = float_to_bits(CONFIG_DEFAULT_RATIO_F);
    }
    if (cfg_with_crc.ch1_ratio_bits == 0U) {
        cfg_with_crc.ch1_ratio_bits = float_to_bits(CONFIG_DEFAULT_RATIO_F);
    }
    if (cfg_with_crc.ch0_thr_bits == 0U) {
        cfg_with_crc.ch0_thr_bits = float_to_bits(CONFIG_DEFAULT_THR_F);
    }
    if (cfg_with_crc.ch1_thr_bits == 0U) {
        cfg_with_crc.ch1_thr_bits = float_to_bits(CONFIG_DEFAULT_THR_F);
    }
    if (cfg_with_crc.dac_value > 4095U) {
        cfg_with_crc.dac_value = 4095U;
    }
    cfg_with_crc.crc32 = calc_crc32((const uint8_t *)&cfg_with_crc, offsetof(config_data_t, crc32));

    /* 擦除DATA区域第一页 */
    fmc_state_enum status;
    fmc_unlock();
    status = fmc_page_erase(CONFIG_DATA_ADDR);
    if (status != FMC_READY) {
        fmc_lock();
        return false;
    }

    /* 写入配置数据 */
    const uint32_t *src = (const uint32_t *)&cfg_with_crc;
    uint32_t addr = CONFIG_DATA_ADDR;
    uint32_t i;
    for (i = 0; i < sizeof(config_data_t) / 4; i++) {
        status = fmc_word_program(addr, src[i]);
        if (status != FMC_READY) {
            fmc_lock();
            return false;
        }
        addr += 4;
    }

    fmc_lock();
    return true;
}

/* 解析INI文件中的值 */
static bool parse_ini_value(const char *content, const char *key, uint32_t *value)
{
    char *pos;
    char line[64];
    char *line_ptr;
    char *eq_pos;

    pos = strstr(content, key);
    if (pos == NULL) {
        return false;
    }

    /* 找到该行的开头 */
    line_ptr = pos;
    while (line_ptr > content && *(line_ptr - 1) != '\n') {
        line_ptr--;
    }

    /* 复制该行 */
    uint32_t i = 0;
    while (*line_ptr != '\r' && *line_ptr != '\n' && *line_ptr != '\0' && i < sizeof(line) - 1) {
        line[i++] = *line_ptr++;
    }
    line[i] = '\0';

    /* 查找等号 */
    eq_pos = strchr(line, '=');
    if (eq_pos == NULL) {
        return false;
    }

    /* 解析值 */
    *value = (uint32_t)atoi(eq_pos + 1);
    return true;
}

/* ADC值转换为电压（12位ADC，参考电压3.3V） */
static float adc_to_voltage(uint16_t adc_val)
{
    return (float)adc_val * 3.3f / 4096.0f;
}

static uint8_t hex_nibble(uint8_t ch)
{
    if(ch >= '0' && ch <= '9') return ch - '0';
    if(ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return 0xFF;
}

static uint8_t ascii_frame_prefix(const uint8_t *data, uint16_t len, const char *prefix)
{
    uint16_t i = 0;
    uint16_t j = 0;
    uint8_t hi, lo;

    while(data && i < len && prefix[j] != '\0'){
        if(data[i] == ' ' || data[i] == '\r' || data[i] == '\n' || data[i] == '\t'){
            i++;
            continue;
        }
        if(prefix[j] == ' '){
            j++;
            continue;
        }
        hi = hex_nibble(data[i]);
        lo = hex_nibble((uint8_t)prefix[j]);
        if(hi == 0xFF || lo == 0xFF || hi != lo) return 0;
        i++;
        j++;
    }

    return prefix[j] == '\0' ? 1U : 0U;
}

static uint16_t ascii_to_bytes(const uint8_t *src, uint16_t len, uint8_t *out, uint16_t out_size)
{
    uint16_t i, n = 0;
    uint8_t have_hi = 0;
    uint8_t hi = 0;
    uint8_t val;

    if(!src || !out) return 0;

    for(i = 0; i < len; i++){
        if(src[i] == ' ' || src[i] == '\r' || src[i] == '\n' || src[i] == '\t'){
            continue;
        }
        val = hex_nibble(src[i]);
        if(val == 0xFF) return 0;
        if(!have_hi){
            hi = val;
            have_hi = 1U;
        } else {
            if(n >= out_size) return 0;
            out[n++] = (uint8_t)((hi << 4) | val);
            have_hi = 0U;
        }
    }

    return have_hi ? 0U : n;
}

static uint16_t sys_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    uint16_t i;
    uint8_t bit;

    for(i = 0; i < len; i++){
        crc ^= data[i];
        for(bit = 0; bit < 8; bit++){
            if(crc & 0x0001U) crc = (uint16_t)((crc >> 1) ^ 0xA001U);
            else crc >>= 1;
        }
    }
    return crc;
}

static uint16_t sys_device_id_get(void)
{
    config_data_t cfg;

    if(config_read(&cfg) && device_id_valid(cfg.device_id)){
        return (uint16_t)cfg.device_id;
    }
    return DEVICE_ID_DEFAULT;
}

static bool sys_device_id_set(uint16_t id)
{
    config_data_t cfg;

    if(!device_id_valid(id)) return false;
    if(!config_read(&cfg)){
        config_default(&cfg);
    }
    cfg.device_id = id;
    return config_write(&cfg);
}

static void config_read_or_default(config_data_t *cfg)
{
    if(!config_read(cfg)){
        config_default(cfg);
    }
}

static uint8_t dec_to_bcd(uint8_t dec)
{
    return (uint8_t)(((dec / 10U) << 4) | (dec % 10U));
}

static uint8_t is_leap_year(uint16_t year)
{
    return ((year % 4U) == 0U && (year % 100U) != 0U) || ((year % 400U) == 0U);
}

static uint8_t month_days(uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {31U,28U,31U,30U,31U,30U,31U,31U,30U,31U,30U,31U};
    if(month == 2U && is_leap_year(year)) return 29U;
    return days[month - 1U];
}

static uint32_t rtc_to_unix(void)
{
    rtc_parameter_struct t;
    uint16_t year;
    uint8_t month;
    uint32_t days = 0U;
    uint16_t y;
    uint8_t m;

    rtc_current_time_get(&t);
    year = (uint16_t)(2000U + bcd_to_dec(t.year));
    month = bcd_to_dec(t.month);

    for(y = 1970U; y < year; y++){
        days += is_leap_year(y) ? 366U : 365U;
    }
    for(m = 1U; m < month; m++){
        days += month_days(year, m);
    }
    days += (uint32_t)(bcd_to_dec(t.date) - 1U);

    return days * 86400U +
           (uint32_t)bcd_to_dec(t.hour) * 3600U +
           (uint32_t)bcd_to_dec(t.minute) * 60U +
           (uint32_t)bcd_to_dec(t.second);
}

static bool unix_to_rtc(uint32_t utc)
{
    rtc_parameter_struct t;
    uint32_t days = utc / 86400U;
    uint32_t rem = utc % 86400U;
    uint16_t year = 1970U;
    uint8_t month = 1U;
    uint8_t dow;

    while(days >= (uint32_t)(is_leap_year(year) ? 366U : 365U)){
        days -= is_leap_year(year) ? 366U : 365U;
        year++;
    }
    while(days >= month_days(year, month)){
        days -= month_days(year, month);
        month++;
    }

    dow = (uint8_t)(((utc / 86400U) + 4U) % 7U);
    if(dow == 0U) dow = RTC_SUNDAY;

    memset(&t, 0, sizeof(t));
    t.factor_asyn = 0x7F;
    t.factor_syn = 0xFF;
    t.year = dec_to_bcd((uint8_t)(year % 100U));
    t.month = dec_to_bcd(month);
    t.date = dec_to_bcd((uint8_t)(days + 1U));
    t.day_of_week = dow;
    t.display_format = RTC_24HOUR;
    t.am_pm = RTC_AM;
    t.hour = dec_to_bcd((uint8_t)(rem / 3600U));
    rem %= 3600U;
    t.minute = dec_to_bcd((uint8_t)(rem / 60U));
    t.second = dec_to_bcd((uint8_t)(rem % 60U));

    return rtc_init(&t) == SUCCESS;
}

static uint32_t baud_from_code(uint8_t code)
{
    switch(code){
        case 0x11U: return 19200U;
        case 0x12U: return 38400U;
        case 0x13U: return 57600U;
        case 0x14U: return 115200U;
        default: return 0U;
    }
}

static uint8_t baud_code_valid(uint8_t code)
{
    return baud_from_code(code) != 0U;
}

static void usart1_set_baud_code(uint8_t code)
{
    uint32_t baud = baud_from_code(code);
    if(baud == 0U) return;
    usart_disable(USART1);
    usart_baudrate_set(USART1, baud);
    usart_enable(USART1);
}

static void put_u32_be(uint8_t *out, uint32_t v)
{
    out[0] = (uint8_t)(v >> 24);
    out[1] = (uint8_t)(v >> 16);
    out[2] = (uint8_t)(v >> 8);
    out[3] = (uint8_t)v;
}

static uint32_t get_u32_be(const uint8_t *in)
{
    return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) | ((uint32_t)in[2] << 8) | in[3];
}

static uint16_t get_u16_be(const uint8_t *in)
{
    return (uint16_t)(((uint16_t)in[0] << 8) | in[1]);
}

static void put_float_be(uint8_t *out, float f)
{
    union { float f; uint32_t u; } v;
    v.f = f;
    put_u32_be(out, v.u);
}

static float get_float_be(const uint8_t *in)
{
    union { float f; uint32_t u; } v;
    v.u = get_u32_be(in);
    return v.f;
}

static float bits_to_float(uint32_t bits)
{
    union { float f; uint32_t u; } v;
    v.u = bits;
    return v.f;
}

static uint32_t float_to_bits(float f)
{
    union { float f; uint32_t u; } v;
    v.f = f;
    return v.u;
}

/* 写告警记录到外部Flash */
static void alarm_record_write(uint8_t channel, float threshold, float actual)
{
    alarm_record_t rec;
    uint32_t addr;

    rec.timestamp = rtc_to_unix();
    rec.channel = channel;
    rec.reserved[0] = rec.reserved[1] = rec.reserved[2] = 0;
    rec.threshold = threshold;
    rec.actual = actual;

    addr = ALARM_FLASH_ADDR + (alarm_wr_index * sizeof(alarm_record_t));
    spi_flash_buffer_write((uint8_t*)&rec, addr, sizeof(alarm_record_t));

    alarm_wr_index = (alarm_wr_index + 1) % ALARM_RECORD_MAX;
    if(alarm_count < ALARM_RECORD_MAX){
        alarm_count++;
    }
}

/* 读告警记录从外部Flash（倒序，最新的在前） */
static uint16_t alarm_record_read(alarm_record_t *buf, uint16_t max_count)
{
    uint16_t i, actual_count, rd_index;
    uint32_t addr;

    actual_count = (alarm_count < max_count) ? alarm_count : max_count;
    if(actual_count == 0){
        return 0;
    }

    for(i = 0; i < actual_count; i++){
        /* 倒序：从最新的开始读 */
        if(alarm_wr_index == 0){
            rd_index = ALARM_RECORD_MAX - 1 - i;
        } else {
            rd_index = alarm_wr_index - 1 - i;
            if(rd_index >= ALARM_RECORD_MAX){  /* 下溢回绕 */
                rd_index += ALARM_RECORD_MAX;
            }
        }
        addr = ALARM_FLASH_ADDR + (rd_index * sizeof(alarm_record_t));
        spi_flash_buffer_read((uint8_t*)&buf[i], addr, sizeof(alarm_record_t));
    }

    return actual_count;
}

/* 清除告警记录 */
static void alarm_record_clear(void)
{
    uint8_t erase_buf[4096];
    memset(erase_buf, 0xFF, sizeof(erase_buf));
    spi_flash_sector_erase(ALARM_FLASH_ADDR);  /* 擦除第一个扇区 */
    alarm_wr_index = 0;
    alarm_count = 0;
}

static float cfg_ch0_ratio(void)
{
    config_data_t cfg;
    config_read_or_default(&cfg);
    return bits_to_float(cfg.ch0_ratio_bits);
}

static float cfg_ch1_ratio(void)
{
    config_data_t cfg;
    config_read_or_default(&cfg);
    return bits_to_float(cfg.ch1_ratio_bits);
}

static float sys_ch0_voltage(void)
{
    return adc_to_voltage(adc_value[0]) * cfg_ch0_ratio();
}

static float sys_ch1_voltage(void)
{
    return adc_to_voltage(g_dac_val) * cfg_ch1_ratio();
}

static float sys_temperature_value(void)
{
    return GD30AD3344_AD_Read(GD30AD3344_Channel_4, GD30AD3344_PGA_4V096);
}

static void sys_dac_apply(uint16_t value)
{
    if(value > 4095U) value = 4095U;
    g_dac_val = value;
    convertarr[0] = value;
    dac_data_set(DAC0, DAC_OUT0, DAC_ALIGN_12B_R, value);
}

static void sys_apply_persistent_config(void)
{
    config_data_t cfg;
    config_read_or_default(&cfg);
    sys_dac_apply((uint16_t)cfg.dac_value);
    usart1_set_baud_code((uint8_t)cfg.baud_code);
}

static void sys_send_frame(uint32_t usart, uint16_t dev_id, uint8_t type,
                           uint16_t cmd, const uint8_t *payload, uint8_t payload_len)
{
    uint8_t raw[300];
    char out[700];
    uint16_t crc;
    uint8_t i;
    int n;
    uint16_t pos = 0;

    raw[pos++] = SYS_HEAD_HI;
    raw[pos++] = SYS_HEAD_LO;
    raw[pos++] = (uint8_t)(dev_id >> 8);
    raw[pos++] = (uint8_t)dev_id;
    raw[pos++] = type;
    raw[pos++] = (uint8_t)(cmd >> 8);
    raw[pos++] = (uint8_t)cmd;
    raw[pos++] = payload_len;
    raw[pos++] = SYS_FRAME_FIXED;
    for(i = 0; i < payload_len && pos < sizeof(raw); i++){
        raw[pos++] = payload[i];
    }

    crc = sys_crc16(raw, pos);
    raw[pos++] = (uint8_t)(crc >> 8);
    raw[pos++] = (uint8_t)crc;
    raw[pos++] = SYS_TAIL_HI;
    raw[pos++] = SYS_TAIL_LO;

    n = snprintf(out, sizeof(out), "A5B6%04X%02X%04X%02X02", dev_id, type, cmd, payload_len);
    if(payload_len > 0U){
        for(i = 0; i < payload_len; i++){
            n += snprintf(&out[n], sizeof(out) - n, "%02X", payload[i]);
        }
    }
    snprintf(&out[n], sizeof(out) - n, "%04XB6A5\r\n", crc);
    my_printf(usart, "%s", out);
}

static void sys_send_error(uint32_t usart, uint16_t dev_id)
{
    sys_send_frame(usart, dev_id, SYS_TYPE_ERR, SYS_CMD_ERR, NULL, 0U);
}

static void boot_request_reply_and_reset(uint32_t usart, uint16_t dev_id, uint16_t cmd);

static uint8_t sys_process_frame(uint32_t usart, const uint8_t *data, uint16_t len)
{
    uint8_t raw[300];
    uint16_t raw_len;
    uint16_t dev_id, cur_id, cmd, crc_rx, crc_calc;
    uint8_t type, payload_len;
    const uint8_t *payload;
    uint8_t rsp[12];
    config_data_t cfg;
    uint8_t ok;

    raw_len = ascii_to_bytes(data, len, raw, sizeof(raw));
    if(raw_len == 0U){
        return ascii_frame_prefix(data, len, "A5B6") ? 1U : 0U;
    }

    cur_id = sys_device_id_get();
    if(raw_len < SYS_FRAME_MIN_LEN || raw[0] != SYS_HEAD_HI || raw[1] != SYS_HEAD_LO){
        sys_send_error(usart, cur_id);
        return 1U;
    }

    dev_id = ((uint16_t)raw[2] << 8) | raw[3];
    if(raw[raw_len - 2U] != SYS_TAIL_HI || raw[raw_len - 1U] != SYS_TAIL_LO){
        sys_send_error(usart, cur_id);
        return 1U;
    }

    payload_len = raw[7];
    cmd = ((uint16_t)raw[5] << 8) | raw[6];
    if(raw[8] != SYS_FRAME_FIXED ||
       (raw_len != (uint16_t)(SYS_FRAME_MIN_LEN + payload_len) &&
        !(cmd == SYS_CMD_OTA_DATA && payload_len == 0U && raw_len == (uint16_t)(SYS_FRAME_MIN_LEN + 256U)))){
        sys_send_error(usart, cur_id);
        return 1U;
    }

    crc_rx = ((uint16_t)raw[raw_len - 4U] << 8) | raw[raw_len - 3U];
    crc_calc = sys_crc16(raw, raw_len - 4U);
    if(crc_rx != crc_calc){
        sys_send_error(usart, cur_id);
        return 1U;
    }

    type = raw[4];
    payload = &raw[9];

    /* 处理心跳帧（0x05类型，0xFFFF命令） */
    if(type == SYS_TYPE_RESET && cmd == 0xFFFFU){
        /* 响应心跳，返回设备ID */
        rsp[0] = (uint8_t)(cur_id >> 8);
        rsp[1] = (uint8_t)cur_id;
        sys_send_frame(usart, cur_id, SYS_TYPE_RESET, 0xFFFFU, rsp, 2U);
        return 1U;
    }

    if(type != SYS_TYPE_REQ || (dev_id != cur_id && dev_id != DEVICE_ID_BROADCAST)){
        return 1U;
    }

    /* 自动上报期间：除停止上报(0x0303)外，其他命令一律不响应(H-02) */
    if(sampling_active && cmd != SYS_CMD_AUTO_STOP){
        return 1U;
    }

    /* 开始定时自动上报(0x0302)：首次立即回一帧，之后由sampling_process按间隔发送 */
    if(cmd == SYS_CMD_AUTO_REPORT && payload_len == 0U){
        uint8_t rep[12];
        float ch0v = sys_ch0_voltage();
        float ch1v = sys_ch1_voltage();
        put_u32_be(rep, rtc_to_unix());
        put_float_be(&rep[4], ch0v);
        put_float_be(&rep[8], ch1v);
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_AUTO_REPORT, rep, 12U);
        sampling_active = 1U;
        sampling_last_time = get_system_ms();
        return 1U;
    }

    /* 停止定时自动上报(0x0303)：回OK并停止 */
    if(cmd == SYS_CMD_AUTO_STOP && payload_len == 0U){
        sampling_active = 0U;
        rsp[0] = 0xFFU;
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_AUTO_STOP, rsp, 1U);
        return 1U;
    }

    if(cmd == SYS_CMD_HANDSHAKE && payload_len == 0U){
        rsp[0] = 0xFFU;
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_HANDSHAKE, rsp, 1U);
        return 1U;
    }

    if(cmd == SYS_CMD_VER_QUERY && payload_len == 0U){
        rsp[0] = FW_VER_MAJOR;
        rsp[1] = FW_VER_MINOR;
        rsp[2] = FW_VER_REVISION;
        rsp[3] = FW_VER_BUILD;
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_VER_QUERY, rsp, 4U);
        return 1U;
    }

    if(cmd == SYS_CMD_TIME_SET && payload_len == 4U){
        rsp[0] = unix_to_rtc(get_u32_be(payload)) ? 0xFFU : 0xFEU;
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_TIME_SET, rsp, 1U);
        return 1U;
    }

    if(cmd == SYS_CMD_TIME_QUERY && payload_len == 0U){
        put_u32_be(rsp, rtc_to_unix());
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_TIME_QUERY, rsp, 4U);
        return 1U;
    }

    if(cmd == SYS_CMD_ID_QUERY && payload_len == 0U){
        rsp[0] = (uint8_t)(cur_id >> 8);
        rsp[1] = (uint8_t)cur_id;
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_ID_QUERY, rsp, 2U);
        return 1U;
    }

    if(cmd == SYS_CMD_BAUD_QUERY && payload_len == 0U){
        config_read_or_default(&cfg);
        rsp[0] = (uint8_t)cfg.baud_code;
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_BAUD_QUERY, rsp, 1U);
        return 1U;
    }

    if(cmd == SYS_CMD_ID_SET && payload_len == 2U && dev_id == cur_id){
        uint16_t new_id = get_u16_be(payload);
        if(sys_device_id_set(new_id)){
            rsp[0] = 0xFFU;
            sys_send_frame(usart, new_id, SYS_TYPE_RSP, SYS_CMD_ID_SET, rsp, 1U);
        } else {
            rsp[0] = 0xFEU;
            sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_ID_SET, rsp, 1U);
        }
        return 1U;
    }

    if(cmd == SYS_CMD_BAUD_SET && payload_len == 1U && dev_id == cur_id){
        ok = baud_code_valid(payload[0]);
        if(ok){
            config_read_or_default(&cfg);
            cfg.baud_code = payload[0];
            ok = config_write(&cfg) ? 1U : 0U;
        }
        rsp[0] = ok ? 0xFFU : 0xFEU;
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_BAUD_SET, rsp, 1U);
        if(ok){
            delay_ms(50);
            usart1_set_baud_code(payload[0]);
        }
        return 1U;
    }

    if(cmd == SYS_CMD_CH0_QUERY && payload_len == 0U){
        put_float_be(rsp, sys_ch0_voltage());
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_CH0_QUERY, rsp, 4U);
        return 1U;
    }

    if(cmd == SYS_CMD_CH1_QUERY && payload_len == 0U){
        put_float_be(rsp, sys_ch1_voltage());
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_CH1_QUERY, rsp, 4U);
        return 1U;
    }

    if(cmd == SYS_CMD_CH2_QUERY && payload_len == 0U){
        put_float_be(rsp, sys_temperature_value());
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_CH2_QUERY, rsp, 4U);
        return 1U;
    }

    /* 0x0241 设CH0变比 */
    if(cmd == SYS_CMD_TEMP_OR_LIMIT && payload_len == 4U){
        float ratio = get_float_be(payload);
        config_read_or_default(&cfg);
        cfg.ch0_ratio_bits = float_to_bits(ratio);
        rsp[0] = config_write(&cfg) ? 0xFFU : 0xFEU;
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_TEMP_OR_LIMIT, rsp, 1U);
        return 1U;
    }

    /* 0x0242 设CH1变比 */
    if(cmd == SYS_CMD_CH1_RATIO_SET && payload_len == 4U){
        float ratio = get_float_be(payload);
        config_read_or_default(&cfg);
        cfg.ch1_ratio_bits = float_to_bits(ratio);
        rsp[0] = config_write(&cfg) ? 0xFFU : 0xFEU;
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_CH1_RATIO_SET, rsp, 1U);
        return 1U;
    }

    /* 0x0400 批量读阈值(CH0+CH1) 返回8字节 */
    if(cmd == SYS_CMD_THR_QUERY_ALL && payload_len == 0U){
        config_read_or_default(&cfg);
        put_float_be(&rsp[0], bits_to_float(cfg.ch0_thr_bits));
        put_float_be(&rsp[4], bits_to_float(cfg.ch1_thr_bits));
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_THR_QUERY_ALL, rsp, 8U);
        return 1U;
    }

    /* 0x0401 读CH0阈值 */
    if(cmd == SYS_CMD_CH0_THR_QUERY && payload_len == 0U){
        config_read_or_default(&cfg);
        put_float_be(rsp, bits_to_float(cfg.ch0_thr_bits));
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_CH0_THR_QUERY, rsp, 4U);
        return 1U;
    }

    /* 0x0402 读CH1阈值 */
    if(cmd == SYS_CMD_CH1_THR_QUERY && payload_len == 0U){
        config_read_or_default(&cfg);
        put_float_be(rsp, bits_to_float(cfg.ch1_thr_bits));
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_CH1_THR_QUERY, rsp, 4U);
        return 1U;
    }

    /* 0x0411 写CH0阈值 */
    if(cmd == SYS_CMD_CH0_THR_SET && payload_len == 4U){
        float thr = get_float_be(payload);
        config_read_or_default(&cfg);
        cfg.ch0_thr_bits = float_to_bits(thr);
        rsp[0] = config_write(&cfg) ? 0xFFU : 0xFEU;
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_CH0_THR_SET, rsp, 1U);
        return 1U;
    }

    /* 0x0412 写CH1阈值 */
    if(cmd == SYS_CMD_CH1_THR_SET && payload_len == 4U){
        float thr = get_float_be(payload);
        config_read_or_default(&cfg);
        cfg.ch1_thr_bits = float_to_bits(thr);
        rsp[0] = config_write(&cfg) ? 0xFFU : 0xFEU;
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_CH1_THR_SET, rsp, 1U);
        return 1U;
    }

    if(cmd == SYS_CMD_PERIOD_SET && payload_len == 1U){
        uint32_t sec = 0U;
        if(payload[0] == 0x01U) sec = 1U;
        else if(payload[0] == 0x02U) sec = 3U;
        else if(payload[0] == 0x03U) sec = 5U;
        if(sec != 0U){
            config_read_or_default(&cfg);
            cfg.ch0_period_sec = sec;
            cfg.ch1_period_sec = sec;
            cfg.limit = sec;
            ok = config_write(&cfg) ? 1U : 0U;
            if(ok){
                sampling_cycle_ms = sec * 1000U;
                sampling_active = 1U;
                sampling_last_time = get_system_ms();
            }
        } else {
            ok = 0U;
        }
        rsp[0] = ok ? 0xFFU : 0xFEU;
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_PERIOD_SET, rsp, 1U);
        return 1U;
    }

    /* 0x0601 设置告警上报模式 */
    if(cmd == SYS_CMD_ALARM_SET && payload_len == 1U){
        if(payload[0] == 0x01U || payload[0] == 0x02U){
            alarm_mode = payload[0];
            rsp[0] = 0xFFU;
        } else {
            rsp[0] = 0xFEU;
        }
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_ALARM_SET, rsp, 1U);
        return 1U;
    }

    /* 0x0602 查询告警记录（最近10条，倒序） */
    if(cmd == SYS_CMD_ALARM_QUERY && payload_len == 0U){
        alarm_record_t records[10];
        uint16_t count, i;
        rtc_parameter_struct rtc_time;
        uint32_t ts;

        count = alarm_record_read(records, 10);
        if(count == 0){
            my_printf(usart, "empty\r\n");
        } else {
            for(i = 0; i < count; i++){
                /* 将时间戳转换回RTC格式显示 */
                unix_to_rtc(records[i].timestamp);
                rtc_current_time_get(&rtc_time);
                my_printf(usart, "20%02x-%02x-%02x %02x:%02x:%02x | CH%d | %.2f | %.2f\r\n",
                          rtc_time.year, rtc_time.month, rtc_time.date,
                          rtc_time.hour, rtc_time.minute, rtc_time.second,
                          records[i].channel,
                          records[i].threshold,
                          records[i].actual);
            }
        }
        return 1U;
    }

    /* 0x0603 清除告警记录 */
    if(cmd == SYS_CMD_ALARM_CLEAR && payload_len == 0U){
        alarm_record_clear();
        rsp[0] = 0xFFU;
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_ALARM_CLEAR, rsp, 1U);
        return 1U;
    }

    if(cmd == SYS_CMD_DAC_SET && payload_len == 2U){
        uint16_t dac_value = get_u16_be(payload);
        ok = dac_value <= 4095U;
        if(ok){
            config_read_or_default(&cfg);
            cfg.dac_value = dac_value;
            ok = config_write(&cfg) ? 1U : 0U;
            if(ok) sys_dac_apply(dac_value);
        }
        rsp[0] = ok ? 0xFFU : 0xFEU;
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_DAC_SET, rsp, 1U);
        return 1U;
    }

    if(cmd == SYS_CMD_SLEEP && payload_len == 0U){
        rsp[0] = 0xFFU;
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_SLEEP, rsp, 1U);

        /* 等待发送完成 */
        delay_1ms(100);

        /* 停止采样 */
        sampling_active = 0U;

        /* 配置RTC闹钟10秒后唤醒 */
        bsp_rtc_alarm_set(10);

        /* 设置睡眠标志 */
        sys_sleeping = 1U;

        /* 进入Stop模式 */
        bsp_enter_stop_mode();

        /* 唤醒后发送字符串 */
        if(rtc_alarm_wakeup_flag){
            rtc_alarm_wakeup_flag = 0;
            my_printf(usart, "instrument wakeup\r\n");
        }

        return 1U;
    }

    if((cmd == SYS_CMD_BOOT_ENTER || cmd == SYS_CMD_RESET) && payload_len == 0U && dev_id == cur_id){
        boot_request_reply_and_reset(usart, cur_id, cmd);
        return 1U;
    }

    if(cmd == SYS_CMD_OTA_DATA || cmd == SYS_CMD_OTA_DONE){
        rsp[0] = 0xFEU;
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, cmd, rsp, 1U);
        return 1U;
    }

    sys_send_error(usart, cur_id);
    return 1U;
}

static void boot_request_mark(void)
{
    rcu_periph_clock_enable(RCU_PMU);
    pmu_backup_write_enable();
    RTC_BKP1 = BOOT_REQ_MAGIC;
}

static void boot_request_reply_and_reset(uint32_t usart, uint16_t dev_id, uint16_t cmd)
{
    uint8_t rsp = 0xFFU;
    sys_send_frame(usart, dev_id, SYS_TYPE_RSP, cmd, &rsp, 1U);
    boot_request_mark();
    delay_ms(50);
    __disable_irq();
    NVIC_SystemReset();
}

uint8_t sampling_is_active(void)
{
    return sampling_active ? 1U : 0U;
}

/* 启动周期采样 */
static void sampling_start(void)
{
    uint32_t cycle_sec;

    /* 从Flash读取配置，获取采样周期 */
    config_data_t cfg;
    if (config_read(&cfg)) {
        /* 使用Limit作为采样周期（秒） */
        sampling_cycle_ms = cfg.limit * 1000;
        cycle_sec = cfg.limit;
    } else {
        /* 默认5秒 */
        sampling_cycle_ms = 5000;
        cycle_sec = 5;
    }

    sampling_active = 1;
    sampling_last_time = get_system_ms();

    my_printf(USART1, "Periodic Sampling\r\n");
    my_printf(USART1, "sample cycle:  %lus\r\n", cycle_sec);
}

/* 停止周期采样 */
static void sampling_stop(void)
{
    sampling_active = 0;
    my_printf(USART1, "Sampling stopped.\r\n");
}

/* 周期采样处理（在uart_task中调用） */
static void sampling_process(void)
{
    uint32_t now;
    uint32_t elapsed;
    float ch0;
    float ch1;
    config_data_t cfg;
    uint8_t payload[12];
    uint8_t alarm_payload[4];
    uint8_t new_alarm_state = 0U;
    uint16_t dev_id;

    config_read_or_default(&cfg);
    ch0 = sys_ch0_voltage();
    ch1 = sys_ch1_voltage();
    dev_id = sys_device_id_get();

    if(ch0 > bits_to_float(cfg.ch0_thr_bits)){
        new_alarm_state |= 0x01U;
    }
    if(ch1 > bits_to_float(cfg.ch1_thr_bits)){
        new_alarm_state |= 0x02U;
    }

    /* CH0告警：状态变化（新告警触发） */
    if((new_alarm_state & 0x01U) != (alarm_state & 0x01U) && (new_alarm_state & 0x01U)){
        float thr = bits_to_float(cfg.ch0_thr_bits);
        alarm_record_write(0, thr, ch0);  /* 存储记录 */

        if(alarm_mode == ALARM_MODE_REPORT){
            /* 主动上报：格式化字符串 */
            rtc_parameter_struct rtc_time;
            rtc_current_time_get(&rtc_time);
            my_printf(USART1, "20%02x-%02x-%02x %02x:%02x:%02x | CH0 | %.2f | %.2f\r\n",
                      rtc_time.year, rtc_time.month, rtc_time.date,
                      rtc_time.hour, rtc_time.minute, rtc_time.second,
                      thr, ch0);
        }
    }

    /* CH1告警：状态变化（新告警触发） */
    if((new_alarm_state & 0x02U) != (alarm_state & 0x02U) && (new_alarm_state & 0x02U)){
        float thr = bits_to_float(cfg.ch1_thr_bits);
        alarm_record_write(1, thr, ch1);  /* 存储记录 */

        if(alarm_mode == ALARM_MODE_REPORT){
            /* 主动上报：格式化字符串 */
            rtc_parameter_struct rtc_time;
            rtc_current_time_get(&rtc_time);
            my_printf(USART1, "20%02x-%02x-%02x %02x:%02x:%02x | CH1 | %.2f | %.2f\r\n",
                      rtc_time.year, rtc_time.month, rtc_time.date,
                      rtc_time.hour, rtc_time.minute, rtc_time.second,
                      thr, ch1);
        }
    }
    alarm_state = new_alarm_state;

    if (!sampling_active) {
        return;
    }

    now = get_system_ms();
    elapsed = now - sampling_last_time;
    if (elapsed >= sampling_cycle_ms) {
        sampling_last_time = now;
        put_u32_be(payload, rtc_to_unix());
        put_float_be(&payload[4], ch0);
        put_float_be(&payload[8], ch1);
        sys_send_frame(USART1, dev_id, SYS_TYPE_RSP, SYS_CMD_AUTO_REPORT, payload, 12U);
    }
}

/* 处理conf命令 */
static void config_manage(void)
{
    FIL file;
    FRESULT res;
    UINT br;
    char buf[256];
    config_data_t cfg = {0};
    uint32_t ratio, limit;

    my_printf(USART1, "-----config manage-----\r\n");

    /* 挂载文件系统 */
    res = f_mount(0, &config_fs);
    if (res != FR_OK) {
        my_printf(USART1, "SD mount fail\r\n");
        my_printf(USART1, "-----config manage-----\r\n");
        return;
    }

    /* 打开config.ini文件 */
    res = f_open(&file, "0:/config.ini", FA_READ);
    if (res != FR_OK) {
        my_printf(USART1, "config.ini file not found.\r\n");
        my_printf(USART1, "-----config manage-----\r\n");
        return;
    }

    /* 读取文件内容 */
    memset(buf, 0, sizeof(buf));
    res = f_read(&file, buf, sizeof(buf) - 1, &br);
    f_close(&file);

    if (res != FR_OK) {
        my_printf(USART1, "Read config.ini fail\r\n");
        my_printf(USART1, "-----config manage-----\r\n");
        return;
    }

    /* 解析Ratio */
    if (!parse_ini_value(buf, "Ratio", &ratio)) {
        my_printf(USART1, "Parse Ratio fail\r\n");
        my_printf(USART1, "-----config manage-----\r\n");
        return;
    }

    /* 解析Limit */
    if (!parse_ini_value(buf, "Limit", &limit)) {
        my_printf(USART1, "Parse Limit fail\r\n");
        my_printf(USART1, "-----config manage-----\r\n");
        return;
    }

    /* 读取当前配置 */
    config_data_t old_cfg;
    bool has_old = config_read(&old_cfg);

    /* 更新配置 */
    cfg.ratio = ratio;
    cfg.limit = limit;
    cfg.device_id = has_old ? old_cfg.device_id : DEVICE_ID_DEFAULT;

    /* 写入Flash */
    if (!config_write(&cfg)) {
        my_printf(USART1, "Write config fail\r\n");
        my_printf(USART1, "-----config manage-----\r\n");
        return;
    }

    /* 输出结果 */
    my_printf(USART1, "Ratio=%lu\r\n", ratio);
    my_printf(USART1, "Limit=%lu\r\n", limit);

    if (has_old) {
        my_printf(USART1, "Old Ratio=%lu, Old Limit=%lu\r\n", old_cfg.ratio, old_cfg.limit);
    }

    my_printf(USART1, "config update success.\r\n");
    my_printf(USART1, "-----config manage-----\r\n");
}

/*
 * 系统自检
 * 检查Flash ID、TF卡状态、RTC时间
 * 输出到USART1
 */
static void system_selftest(void)
{
    uint32_t flash_id;
    DSTATUS sd_status;
    uint32_t sd_capacity;
    uint8_t year, month, date, hour, minute, second;

    my_printf(USART1, "-----system selftest-----\r\n");

    /* Flash检测：读取ID */
    flash_id = spi_flash_read_id();
    if (flash_id != 0 && flash_id != 0xFFFFFF) {
        my_printf(USART1, "flash........ok\r\n");
    } else {
        my_printf(USART1, "flash........fail\r\n");
    }

    /* TF卡检测：初始化并检查状态 */
    sd_status = disk_initialize(0);
    if (sd_status == 0) {
        my_printf(USART1, "TF card....ok\r\n");
        sd_capacity = sd_card_capacity_get();
    } else {
        my_printf(USART1, "TF card....fail\r\n");
        sd_capacity = 0;
    }

    /* 输出Flash ID */
    my_printf(USART1, "flash ID: 0x%06lX\r\n", flash_id);

    /* 输出TF卡容量 */
    if (sd_capacity > 0) {
        my_printf(USART1, "TF card memory: %lu KB\r\n", sd_capacity);
    } else {
        my_printf(USART1, "TF card memory: N/A\r\n");
    }

    /* 输出RTC时间（BCD转十进制） */
    rtc_current_time_get(&rtc_initpara);
    year = bcd_to_dec(rtc_initpara.year);
    month = bcd_to_dec(rtc_initpara.month);
    date = bcd_to_dec(rtc_initpara.date);
    hour = bcd_to_dec(rtc_initpara.hour);
    minute = bcd_to_dec(rtc_initpara.minute);
    second = bcd_to_dec(rtc_initpara.second);
    my_printf(USART1, "RTC:20%02d-%02d-%02d %02d:%02d:%02d\r\n",
              year, month, date, hour, minute, second);

    my_printf(USART1, "-----system selftest-----\r\n");
}

/* debug帧回调，解析命令 */
void debug_uart_frame_callback(const uint8_t *d, uint16_t len)
{
    if (sys_sleeping) {
        sys_sleeping = 0U;
        my_printf(DEBUG_USART, "instrument wakeup\r\n");
        /* 继续处理该帧，不要return */
    }
    if (sys_process_frame(DEBUG_USART, d, len)) {
        return;
    } else if (len >= 4 && d[0] == 't' && d[1] == 'e' && d[2] == 's' && d[3] == 't') {
        system_selftest();
    } else if (len >= 4 && d[0] == 'c' && d[1] == 'o' && d[2] == 'n' && d[3] == 'f') {
        config_manage();
    } else if (len >= 5 && d[0] == 's' && d[1] == 't' && d[2] == 'a' && d[3] == 'r' && d[4] == 't') {
        sampling_start();
    } else if (len >= 4 && d[0] == 's' && d[1] == 't' && d[2] == 'o' && d[3] == 'p') {
        sampling_stop();
    } else {
        my_printf(DEBUG_USART, "[DBG] %u bytes\r\n", len);
    }
}

/* USART1帧回调，解析命令 */
void usart1_frame_callback(const uint8_t *d, uint16_t len)
{
    if (sys_sleeping) {
        sys_sleeping = 0U;
        my_printf(USART1, "instrument wakeup\r\n");
        /* 继续处理该帧，不要return */
    }
    if (sys_process_frame(USART1, d, len)) {
        return;
    } else if (len >= 4 && d[0] == 't' && d[1] == 'e' && d[2] == 's' && d[3] == 't') {
        system_selftest();
    } else if (len >= 4 && d[0] == 'c' && d[1] == 'o' && d[2] == 'n' && d[3] == 'f') {
        config_manage();
    } else if (len >= 5 && d[0] == 's' && d[1] == 't' && d[2] == 'a' && d[3] == 'r' && d[4] == 't') {
        sampling_start();
    } else if (len >= 4 && d[0] == 's' && d[1] == 't' && d[2] == 'o' && d[3] == 'p') {
        sampling_stop();
    } else {
        my_printf(USART1, "[U1] %u bytes\r\n", len);
    }
}

void uart_task(void)
{
    static uint8_t cfg_applied = 0U;

    if(!cfg_applied){
        sys_apply_persistent_config();
        /* 上电心跳帧，通知上位机设备已就绪 */
        {
            config_data_t hb_cfg;
            config_read_or_default(&hb_cfg);
            sys_send_frame(USART1, (uint16_t)hb_cfg.device_id, SYS_TYPE_RESET, 0x8888U, NULL, 0U);
        }
        cfg_applied = 1U;
    }

    if(debug_rx_flag){
        debug_uart_frame_callback(debug_uart_dma_buffer, debug_uart_dma_len);
        memset(debug_uart_dma_buffer, 0, sizeof(debug_uart_dma_buffer));
        debug_uart_dma_len = 0;
        debug_rx_flag = 0;
    }

    if(usart1_rx_flag){
        usart1_frame_callback(usart1_uart_dma_buffer, usart1_uart_dma_len);
        memset(usart1_uart_dma_buffer, 0, sizeof(usart1_uart_dma_buffer));
        usart1_uart_dma_len = 0;
        usart1_rx_flag = 0;
    }

    /* 周期采样处理 */
    sampling_process();

    ota_dma_poll();
    ota_poll();
}
