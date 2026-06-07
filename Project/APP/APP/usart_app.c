/*
 * 串口任务
 * 处理debug串口帧回调 + ota dma轮询
 * 2026-05 改写
 */
#include "mcu_cmic_gd32f470vet6.h"
#include "usart_app.h"
#include "ota_uart.h"
#include "rtc_app.h"
#include "bl_partition.h"

extern uint8_t rxbuffer[OTA_UART_RXBUF_SIZE];
extern uint8_t debug_rxbuffer[DEBUG_UART_RXBUF_SIZE];
extern uint8_t usart1_rxbuffer[256];
extern rtc_parameter_struct rtc_initpara;
extern uint16_t adc_value[2];

/* 配置数据存储在DATA区域 */
#define CONFIG_DATA_ADDR  BL_DATA_START_ADDR
#define CONFIG_MAGIC      0x434F4E46UL  /* "CONF" */
#define CONFIG_DEFAULT_RATIO  1UL
#define CONFIG_DEFAULT_LIMIT  5UL

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
#define SYS_CMD_VER_QUERY    0x0104U
#define SYS_CMD_ID_QUERY     0x0111U
#define SYS_CMD_ID_SET       0x01A1U
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
    uint32_t magic;      /* CONFIG_MAGIC */
    uint32_t ratio;      /* 变比 */
    uint32_t limit;      /* 阈值/采样周期 */
    uint32_t device_id;  /* 设备ID: 0x0001~0xFFFE */
    uint32_t crc32;      /* CRC32校验 */
} config_data_t;

/* 文件系统变量 */
static FATFS config_fs;

/* 周期采样相关变量 */
static volatile uint8_t sampling_active = 0;      /* 采样状态：0=停止，1=运行 */
static volatile uint32_t sampling_cycle_ms = 5000; /* 采样周期，默认5秒 */
static volatile uint32_t sampling_last_time = 0;   /* 上次采样时间 */

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

/* 从Flash读取配置 */
static void config_default(config_data_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->magic = CONFIG_MAGIC;
    cfg->ratio = CONFIG_DEFAULT_RATIO;
    cfg->limit = CONFIG_DEFAULT_LIMIT;
    cfg->device_id = DEVICE_ID_DEFAULT;
}

static bool device_id_valid(uint32_t id)
{
    return id >= DEVICE_ID_MIN && id <= DEVICE_ID_MAX;
}

static bool config_read(config_data_t *cfg)
{
    const config_data_t *flash_cfg = (const config_data_t *)CONFIG_DATA_ADDR;
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

static void sys_send_frame(uint32_t usart, uint16_t dev_id, uint8_t type,
                           uint16_t cmd, const uint8_t *payload, uint8_t payload_len)
{
    uint8_t raw[48];
    char out[160];
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

    n = snprintf(out, sizeof(out), "A5B6 %04X %02X %04X %02X 02", dev_id, type, cmd, payload_len);
    if(payload_len > 0U){
        n += snprintf(&out[n], sizeof(out) - n, " ");
        for(i = 0; i < payload_len; i++){
            n += snprintf(&out[n], sizeof(out) - n, "%02X", payload[i]);
        }
    }
    snprintf(&out[n], sizeof(out) - n, " %04X B6A5\r\n", crc);
    my_printf(usart, "%s", out);
}

static void sys_send_error(uint32_t usart, uint16_t dev_id)
{
    sys_send_frame(usart, dev_id, SYS_TYPE_ERR, SYS_CMD_ERR, NULL, 0U);
}

static void boot_request_reply_and_reset(uint32_t usart, uint16_t dev_id);

static uint8_t sys_process_frame(uint32_t usart, const uint8_t *data, uint16_t len)
{
    uint8_t raw[64];
    uint16_t raw_len;
    uint16_t dev_id, cur_id, cmd, crc_rx, crc_calc;
    uint8_t type, payload_len;
    const uint8_t *payload;
    uint8_t rsp[4];

    raw_len = ascii_to_bytes(data, len, raw, sizeof(raw));
    if(raw_len == 0U){
        return ascii_frame_prefix(data, len, "A5B6") ? 1U : 0U;
    }
    if(raw_len < SYS_FRAME_MIN_LEN || raw[0] != SYS_HEAD_HI || raw[1] != SYS_HEAD_LO){
        return 0U;
    }
    if(raw[raw_len - 2U] != SYS_TAIL_HI || raw[raw_len - 1U] != SYS_TAIL_LO){
        return 1U;
    }

    payload_len = raw[7];
    if(raw[8] != SYS_FRAME_FIXED || raw_len != (uint16_t)(SYS_FRAME_MIN_LEN + payload_len)){
        sys_send_error(usart, sys_device_id_get());
        return 1U;
    }

    crc_rx = ((uint16_t)raw[raw_len - 4U] << 8) | raw[raw_len - 3U];
    crc_calc = sys_crc16(raw, raw_len - 4U);
    dev_id = ((uint16_t)raw[2] << 8) | raw[3];
    cur_id = sys_device_id_get();
    if(crc_rx != crc_calc){
        sys_send_error(usart, cur_id);
        return 1U;
    }

    type = raw[4];
    cmd = ((uint16_t)raw[5] << 8) | raw[6];
    payload = &raw[9];

    if(type == SYS_TYPE_REQ && cmd == SYS_CMD_VER_QUERY && payload_len == 0U && dev_id == cur_id){
        rsp[0] = FW_VER_MAJOR;
        rsp[1] = FW_VER_MINOR;
        rsp[2] = FW_VER_REVISION;
        rsp[3] = FW_VER_BUILD;
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_VER_QUERY, rsp, 4U);
        return 1U;
    }

    if(type == SYS_TYPE_REQ && cmd == SYS_CMD_ID_QUERY && payload_len == 0U &&
       (dev_id == DEVICE_ID_BROADCAST || dev_id == cur_id)){
        rsp[0] = (uint8_t)(cur_id >> 8);
        rsp[1] = (uint8_t)cur_id;
        sys_send_frame(usart, cur_id, SYS_TYPE_RSP, SYS_CMD_ID_QUERY, rsp, 2U);
        return 1U;
    }

    if(type == SYS_TYPE_REQ && cmd == SYS_CMD_ID_SET && payload_len == 2U && dev_id == cur_id){
        uint16_t new_id = ((uint16_t)payload[0] << 8) | payload[1];
        if(sys_device_id_set(new_id)){
            rsp[0] = 0xFFU;
            sys_send_frame(usart, new_id, SYS_TYPE_RSP, SYS_CMD_ID_SET, rsp, 1U);
        } else {
            sys_send_error(usart, cur_id);
        }
        return 1U;
    }

    if(type == SYS_TYPE_RESET && cmd == SYS_CMD_RESET && payload_len == 0U && dev_id == cur_id){
        boot_request_reply_and_reset(usart, cur_id);
        return 1U;
    }

    if(dev_id == cur_id || dev_id == DEVICE_ID_BROADCAST){
        sys_send_error(usart, cur_id);
    }
    return 1U;
}

static void boot_request_mark(void)
{
    rcu_periph_clock_enable(RCU_PMU);
    pmu_backup_write_enable();
    RTC_BKP1 = BOOT_REQ_MAGIC;
}

static void boot_request_reply_and_reset(uint32_t usart, uint16_t dev_id)
{
    sys_send_frame(usart, dev_id, SYS_TYPE_RESET, SYS_CMD_RESET, NULL, 0U);
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
    uint16_t adc_val;
    float voltage;
    rtc_parameter_struct rtc_time;
    uint8_t year, month, date, hour, minute, second;

    if (!sampling_active) {
        return;
    }

    now = get_system_ms();

    /* 周期采样输出 */
    elapsed = now - sampling_last_time;
    if (elapsed >= sampling_cycle_ms) {
        sampling_last_time = now;

        /* 获取ADC值 */
        adc_val = adc_value[0];
        voltage = adc_to_voltage(adc_val);

        /* 获取RTC时间 */
        rtc_current_time_get(&rtc_time);
        year = bcd_to_dec(rtc_time.year);
        month = bcd_to_dec(rtc_time.month);
        date = bcd_to_dec(rtc_time.date);
        hour = bcd_to_dec(rtc_time.hour);
        minute = bcd_to_dec(rtc_time.minute);
        second = bcd_to_dec(rtc_time.second);

        /* 串口输出 */
        my_printf(USART1, "20%02d-%02d-%02d %02d:%02d:%02d ch0=%.1fV\r\n",
                  year, month, date, hour, minute, second, voltage);
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
