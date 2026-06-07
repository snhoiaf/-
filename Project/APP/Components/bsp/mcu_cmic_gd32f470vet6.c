/*
 * V2板 BSP 初始化
 * 2026-05 重写
 *
 * 各外设的gpio+dma+时钟配置
 * 引脚不变，写法全换了
 */
#include "mcu_cmic_gd32f470vet6.h"

/* ---- 全局缓冲区 ---- */
__IO uint8_t oled_cmd_buf[2]  = {0x00, 0x00};
__IO uint8_t oled_data_buf[2] = {0x40, 0x00};

uint8_t spi3_send_array[ARRAYSIZE], spi3_receive_array[ARRAYSIZE];
uint8_t spi1_send_array[ARRAYSIZE], spi1_receive_array[ARRAYSIZE];

uint8_t rxbuffer[OTA_RX_SZ];
uint8_t debug_rxbuffer[DBG_RX_SZ];
uint8_t usart1_rxbuffer[256];
uint8_t usart5_rxbuffer[256];

uint16_t adc_value[2];
uint16_t convertarr[DAC_N];

rtc_parameter_struct rtc_initpara;
rtc_alarm_struct rtc_alarm;
__IO uint32_t prescaler_a, prescaler_s;
uint32_t RTCSRC_FLAG;

/* ---- LED ---- */

void bsp_led_init(void)
{
    rcu_periph_clock_enable(LED_CLK);
    gpio_mode_set(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP,
                  LED1|LED2|LED3|LED4|LED5|LED6);
    gpio_output_options_set(LED_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            LED1|LED2|LED3|LED4|LED5|LED6);
    GPIO_BC(LED_PORT) = LED1|LED2|LED3|LED4|LED5|LED6;
}

/* ---- 按键 ---- */

void bsp_btn_init(void)
{
    rcu_periph_clock_enable(KB_CLK);
    rcu_periph_clock_enable(KE_CLK);
    rcu_periph_clock_enable(KA_CLK);

    gpio_mode_set(KE_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP,
                  K1_PIN|K2_PIN|K3_PIN|K4_PIN|K5_PIN);
    gpio_mode_set(KB_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, K6_PIN);
    gpio_mode_set(KA_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, KW_PIN);
}

/* ---- USART0, debug ---- */

void bsp_usart0_init(void)
{
    dma_single_data_parameter_struct d;

    rcu_periph_clock_enable(DBG_DMA_RCU);
    dma_deinit(DBG_DMA, DBG_DMA_CH);
    d.direction = DMA_PERIPH_TO_MEMORY;
    d.memory0_addr = (uint32_t)debug_rxbuffer;
    d.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    d.number = DBG_RX_SZ;
    d.periph_addr = DBG_DR;
    d.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    d.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    d.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(DBG_DMA, DBG_DMA_CH, &d);
    dma_circulation_disable(DBG_DMA, DBG_DMA_CH);
    dma_channel_subperipheral_select(DBG_DMA, DBG_DMA_CH, DBG_DMA_SUB);
    dma_channel_enable(DBG_DMA, DBG_DMA_CH);

    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_USART0);

    gpio_af_set(USART0_TX_PORT, AF_USART0, USART0_TX|USART0_RX);
    gpio_mode_set(USART0_TX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART0_TX|USART0_RX);
    gpio_output_options_set(USART0_TX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART0_TX|USART0_RX);

    usart_deinit(USART0);
    usart_baudrate_set(USART0, 115200U);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    usart_dma_receive_config(USART0, USART_RECEIVE_DMA_ENABLE);
    usart_enable(USART0);

    nvic_irq_enable(USART0_IRQn, 0, 0);
    usart_interrupt_enable(USART0, USART_INT_IDLE);
}

/* ---- USART2, OTA ---- */

void bsp_usart2_init(void)
{
    dma_single_data_parameter_struct d;

    rcu_periph_clock_enable(OTA_DMA_RCU);
    rcu_periph_clock_enable(OTA_PORT_RCU);
    rcu_periph_clock_enable(OTA_UART_RCU);

    dma_deinit(OTA_DMA, OTA_DMA_CH);
    d.direction = DMA_PERIPH_TO_MEMORY;
    d.memory0_addr = (uint32_t)rxbuffer;
    d.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    d.number = OTA_RX_SZ;
    d.periph_addr = OTA_DR;
    d.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    d.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    d.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(OTA_DMA, OTA_DMA_CH, &d);
    dma_circulation_enable(OTA_DMA, OTA_DMA_CH);
    dma_channel_subperipheral_select(OTA_DMA, OTA_DMA_CH, OTA_DMA_SUB);
    dma_channel_enable(OTA_DMA, OTA_DMA_CH);

    gpio_af_set(OTA_PORT, OTA_AF, OTA_TX|OTA_RX);
    gpio_mode_set(OTA_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, OTA_TX|OTA_RX);
    gpio_output_options_set(OTA_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, OTA_TX|OTA_RX);

    usart_deinit(OTA_UART);
    usart_baudrate_set(OTA_UART, OTA_BAUD);
    usart_receive_config(OTA_UART, USART_RECEIVE_ENABLE);
    usart_transmit_config(OTA_UART, USART_TRANSMIT_ENABLE);
    usart_dma_receive_config(OTA_UART, USART_RECEIVE_DMA_ENABLE);
    usart_enable(OTA_UART);

    nvic_irq_enable((IRQn_Type)OTA_IRQn, 0, 0);
    usart_interrupt_enable(OTA_UART, USART_INT_IDLE);
}

/* ---- USART1, RS485 ---- */

void bsp_usart1_init(void)
{
    dma_single_data_parameter_struct d;

    rcu_periph_clock_enable(RCU_DMA0);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOE);
    rcu_periph_clock_enable(RCU_USART1);

    /* RS485方向控制引脚初始化 (PE8) */
    gpio_mode_set(GPIOE, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_8);
    gpio_output_options_set(GPIOE, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_8);
    gpio_bit_reset(GPIOE, GPIO_PIN_8);  /* 默认接收模式 */

    dma_deinit(U1_DMA, U1_DMA_CH);
    d.direction = DMA_PERIPH_TO_MEMORY;
    d.memory0_addr = (uint32_t)usart1_rxbuffer;
    d.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    d.number = sizeof(usart1_rxbuffer);
    d.periph_addr = U1_DR;
    d.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    d.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    d.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(U1_DMA, U1_DMA_CH, &d);
    dma_circulation_disable(U1_DMA, U1_DMA_CH);
    dma_channel_subperipheral_select(U1_DMA, U1_DMA_CH, U1_DMA_SUB);
    dma_channel_enable(U1_DMA, U1_DMA_CH);

    gpio_af_set(USART1_TX_PORT, AF_USART1, USART1_TX_PIN|USART1_RX_PIN);
    gpio_mode_set(USART1_TX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART1_TX_PIN|USART1_RX_PIN);
    gpio_output_options_set(USART1_TX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART1_TX_PIN|USART1_RX_PIN);

    usart_deinit(USART1);
    usart_baudrate_set(USART1, 19200U);
    usart_receive_config(USART1, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART1, USART_TRANSMIT_ENABLE);
    usart_dma_receive_config(USART1, USART_RECEIVE_DMA_ENABLE);
    usart_enable(USART1);

    /* 使能USART1空闲中断 */
    nvic_irq_enable(USART1_IRQn, 0, 0);
    usart_interrupt_enable(USART1, USART_INT_IDLE);
}

/* ---- USART5 ---- */

void bsp_usart5_init(void)
{
    dma_single_data_parameter_struct d;

    rcu_periph_clock_enable(RCU_DMA1);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_USART5);

    dma_deinit(U5_DMA, U5_DMA_CH);
    d.direction = DMA_PERIPH_TO_MEMORY;
    d.memory0_addr = (uint32_t)usart5_rxbuffer;
    d.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    d.number = sizeof(usart5_rxbuffer);
    d.periph_addr = U5_DR;
    d.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    d.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    d.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(U5_DMA, U5_DMA_CH, &d);
    dma_circulation_disable(U5_DMA, U5_DMA_CH);
    dma_channel_subperipheral_select(U5_DMA, U5_DMA_CH, U5_DMA_SUB);
    dma_channel_enable(U5_DMA, U5_DMA_CH);

    gpio_af_set(USART5_TX_PORT, AF_USART5, USART5_TX_PIN|USART5_RX_PIN);
    gpio_mode_set(USART5_TX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART5_TX_PIN|USART5_RX_PIN);
    gpio_output_options_set(USART5_TX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART5_TX_PIN|USART5_RX_PIN);

    usart_deinit(USART5);
    usart_baudrate_set(USART5, 115200U);
    usart_receive_config(USART5, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART5, USART_TRANSMIT_ENABLE);
    usart_dma_receive_config(USART5, USART_RECEIVE_DMA_ENABLE);
    usart_enable(USART5);
}

void bsp_usart_init(void)
{
    bsp_usart0_init();
    bsp_usart2_init();
}

void bsp_usart_all_init(void)
{
    bsp_usart0_init();
    bsp_usart1_init();
    bsp_usart2_init();
    bsp_usart5_init();
}

void bsp_ota_dma_rearm(void)
{
    dma_channel_disable(OTA_DMA, OTA_DMA_CH);
    dma_flag_clear(OTA_DMA, OTA_DMA_CH, DMA_FLAG_FTF);
    dma_transfer_number_config(OTA_DMA, OTA_DMA_CH, OTA_RX_SZ);
    dma_channel_enable(OTA_DMA, OTA_DMA_CH);
}

uint32_t bsp_ota_dma_cnt(void)
{
    return OTA_RX_SZ - dma_transfer_number_get(OTA_DMA, OTA_DMA_CH);
}

/* ---- OLED I2C0 ---- */

void bsp_oled_init(void)
{
    dma_single_data_parameter_struct d;

    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_I2C0);
    rcu_periph_clock_enable(RCU_DMA0);

    gpio_af_set(OLED_SDA_PORT, AF_I2C0, OLED_SDA_PIN);
    gpio_af_set(OLED_SCL_PORT, AF_I2C0, OLED_SCL_PIN);
    gpio_mode_set(OLED_SDA_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, OLED_SDA_PIN);
    gpio_output_options_set(OLED_SDA_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, OLED_SDA_PIN);
    gpio_mode_set(OLED_SCL_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, OLED_SCL_PIN);
    gpio_output_options_set(OLED_SCL_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, OLED_SCL_PIN);

    i2c_clock_config(I2C0, 400000, I2C_DTCY_2);
    i2c_mode_addr_config(I2C0, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, I2C_OWN_ADDR);
    i2c_enable(I2C0);
    i2c_ack_config(I2C0, I2C_ACK_ENABLE);

    dma_deinit(DMA0, DMA_CH6);
    dma_single_data_para_struct_init(&d);
    d.direction = DMA_MEMORY_TO_PERIPH;
    d.memory0_addr = (uint32_t)oled_data_buf;
    d.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    d.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    d.number = 2;
    d.periph_addr = I2C_DATA_REG;
    d.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    d.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(DMA0, DMA_CH6, &d);
    dma_circulation_disable(DMA0, DMA_CH6);
    dma_channel_subperipheral_select(DMA0, DMA_CH6, DMA_SUBPERI1);
}

/* ---- SPI Flash ---- */

void bsp_gd25qxx_init(void)
{
    spi_parameter_struct sp;

    rcu_periph_clock_enable(FL_GPIO_CLK);
    rcu_periph_clock_enable(FL_CS_CLK);
    rcu_periph_clock_enable(FL_SPI_CLK);
    rcu_periph_clock_enable(RCU_DMA0);

    gpio_af_set(FL_GPIO_PORT, AF_SPI0, FL_SCK_PIN|FL_MISO_PIN|FL_MOSI_PIN);
    gpio_mode_set(FL_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, FL_SCK_PIN|FL_MISO_PIN|FL_MOSI_PIN);
    gpio_output_options_set(FL_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, FL_SCK_PIN|FL_MISO_PIN|FL_MOSI_PIN);

    gpio_mode_set(FL_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, FL_CS_PIN);
    gpio_output_options_set(FL_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, FL_CS_PIN);

    sp.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    sp.device_mode          = SPI_MASTER;
    sp.frame_size           = SPI_FRAMESIZE_8BIT;
    sp.clock_polarity_phase = SPI_CK_PL_HIGH_PH_2EDGE;
    sp.nss                  = SPI_NSS_SOFT;
    sp.prescale             = SPI_PSC_8;
    sp.endian               = SPI_ENDIAN_MSB;
    spi_init(FL_SPI, &sp);

    spi_flash_init();
}

/* ---- GD30AD3344 ---- */

void bsp_gd30ad3344_init(void)
{
    spi_parameter_struct sp;

    rcu_periph_clock_enable(AD_GPIO_CLK);
    rcu_periph_clock_enable(AD_SPI_CLK);
    rcu_periph_clock_enable(RCU_DMA1);

    gpio_af_set(AD_GPIO_PORT, AF_SPI3, SPI3_SCK_PIN|SPI3_MISO_PIN|SPI3_MOSI_PIN);
    gpio_mode_set(AD_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SPI3_SCK_PIN|SPI3_MISO_PIN|SPI3_MOSI_PIN);
    gpio_output_options_set(AD_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SPI3_SCK_PIN|SPI3_MISO_PIN|SPI3_MOSI_PIN);

    gpio_mode_set(S_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, S_CS_PIN);
    gpio_output_options_set(S_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, S_CS_PIN);

    sp.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    sp.device_mode          = SPI_MASTER;
    sp.frame_size           = SPI_FRAMESIZE_8BIT;
    sp.clock_polarity_phase = SPI_CK_PL_LOW_PH_2EDGE;
    sp.nss                  = SPI_NSS_SOFT;
    sp.prescale             = SPI_PSC_8;
    sp.endian               = SPI_ENDIAN_MSB;
    spi_init(AD_SPI, &sp);

    GD30AD3344_Init();
}

/* ---- ETH ---- */

void bsp_eth_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_SYSCFG);
    rcu_periph_clock_enable(RCU_ENET);
    rcu_periph_clock_enable(RCU_ENETTX);
    rcu_periph_clock_enable(RCU_ENETRX);

    syscfg_enet_phy_interface_config(SYSCFG_ENET_PHY_RMII);

    gpio_af_set(ETH_REFCLK_PORT, AF_ETH, ETH_REFCLK_PIN|ETH_MDIO_PIN|ETH_CRSDV_PIN);
    gpio_mode_set(ETH_REFCLK_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, ETH_REFCLK_PIN|ETH_MDIO_PIN|ETH_CRSDV_PIN);
    gpio_output_options_set(ETH_REFCLK_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, ETH_REFCLK_PIN|ETH_MDIO_PIN|ETH_CRSDV_PIN);

    gpio_af_set(ETH_MDC_PORT, AF_ETH, ETH_MDC_PIN|ETH_RXD0_PIN|ETH_RXD1_PIN);
    gpio_mode_set(ETH_MDC_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, ETH_MDC_PIN|ETH_RXD0_PIN|ETH_RXD1_PIN);
    gpio_output_options_set(ETH_MDC_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, ETH_MDC_PIN|ETH_RXD0_PIN|ETH_RXD1_PIN);

    gpio_af_set(ETH_TXEN_PORT, AF_ETH, ETH_TXEN_PIN|ETH_TXD0_PIN|ETH_TXD1_PIN);
    gpio_mode_set(ETH_TXEN_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, ETH_TXEN_PIN|ETH_TXD0_PIN|ETH_TXD1_PIN);
    gpio_output_options_set(ETH_TXEN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, ETH_TXEN_PIN|ETH_TXD0_PIN|ETH_TXD1_PIN);

    gpio_mode_set(PHY_RST_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, PHY_RST_PIN);
    gpio_output_options_set(PHY_RST_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, PHY_RST_PIN);
    gpio_bit_set(PHY_RST_PORT, PHY_RST_PIN);
}

/* ---- SDIO ---- */

void bsp_sdio_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_SDIO);

    gpio_af_set(SD_CLK_PORT, AF_SDIO, SD_D0_PIN|SD_D1_PIN|SD_D2_PIN|SD_D3_PIN|SD_CLK_PIN);
    gpio_af_set(SD_CMD_PORT, AF_SDIO, SD_CMD_PIN);

    gpio_mode_set(SD_CLK_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, SD_D0_PIN|SD_D1_PIN|SD_D2_PIN|SD_D3_PIN);
    gpio_output_options_set(SD_CLK_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_25MHZ, SD_D0_PIN|SD_D1_PIN|SD_D2_PIN|SD_D3_PIN);

    gpio_mode_set(SD_CLK_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SD_CLK_PIN);
    gpio_output_options_set(SD_CLK_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_25MHZ, SD_CLK_PIN);

    gpio_mode_set(SD_CMD_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, SD_CMD_PIN);
    gpio_output_options_set(SD_CMD_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_25MHZ, SD_CMD_PIN);
}

/* ---- ADC ---- */

void bsp_adc_init(void)
{
    dma_single_data_parameter_struct d;

    rcu_periph_clock_enable(ADC_GPIO_CLK);
    rcu_periph_clock_enable(RCU_ADC0);
    rcu_periph_clock_enable(RCU_DMA1);

    adc_clock_config(ADC_ADCCK_PCLK2_DIV8);
    gpio_mode_set(ADC_GPIO_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, ADC_CH10_PIN|ADC_CH12_PIN);

    dma_deinit(DMA1, DMA_CH0);
    d.periph_addr = (uint32_t)(&ADC_RDATA(ADC0));
    d.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    d.memory0_addr = (uint32_t)adc_value;
    d.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    d.periph_memory_width = DMA_PERIPH_WIDTH_16BIT;
    d.direction = DMA_PERIPH_TO_MEMORY;
    d.number = 2;
    d.priority = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(DMA1, DMA_CH0, &d);
    dma_channel_subperipheral_select(DMA1, DMA_CH0, DMA_SUBPERI0);
    dma_circulation_enable(DMA1, DMA_CH0);
    dma_channel_enable(DMA1, DMA_CH0);

    adc_sync_mode_config(ADC_SYNC_MODE_INDEPENDENT);
    adc_special_function_config(ADC0, ADC_CONTINUOUS_MODE, ENABLE);
    adc_special_function_config(ADC0, ADC_SCAN_MODE, ENABLE);
    adc_data_alignment_config(ADC0, ADC_DATAALIGN_RIGHT);

    adc_channel_length_config(ADC0, ADC_ROUTINE_CHANNEL, 2);
    adc_routine_channel_config(ADC0, 0, ADC_CHANNEL_10, ADC_SAMPLETIME_15);
    adc_routine_channel_config(ADC0, 1, ADC_CHANNEL_12, ADC_SAMPLETIME_15);
    adc_external_trigger_source_config(ADC0, ADC_ROUTINE_CHANNEL, ADC_EXTTRIG_ROUTINE_T0_CH0);
    adc_external_trigger_config(ADC0, ADC_ROUTINE_CHANNEL, EXTERNAL_TRIGGER_DISABLE);

    adc_dma_request_after_last_enable(ADC0);
    adc_dma_mode_enable(ADC0);
    adc_enable(ADC0);
    delay_1ms(1);
    adc_calibration_enable(ADC0);
    adc_software_trigger_enable(ADC0, ADC_ROUTINE_CHANNEL);
}

/* ---- TIMER5, 给DAC做触发源 ---- */

static void tmr5_cfg(void)
{
    timer_parameter_struct t;
    timer_deinit(TIMER5);
    timer_struct_para_init(&t);
    t.prescaler         = 239;
    t.alignedmode       = TIMER_COUNTER_EDGE;
    t.counterdirection  = TIMER_COUNTER_UP;
    t.period            = 99;
    t.clockdivision     = TIMER_CKDIV_DIV1;
    t.repetitioncounter = 0;
    timer_init(TIMER5, &t);
    timer_master_output_trigger_source_select(TIMER5, TIMER_TRI_OUT_SRC_UPDATE);
    timer_enable(TIMER5);
}

/* ---- DAC ---- */

void bsp_dac_init(void)
{
    rcu_periph_clock_enable(DAC_GPIO_CLK);
    rcu_periph_clock_enable(RCU_DAC);
    rcu_periph_clock_enable(RCU_TIMER5);

    gpio_mode_set(DAC_GPIO_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, DAC0_PIN);

    dac_deinit(DAC0);
    dac_trigger_source_config(DAC0, DAC_OUT0, DAC_TRIGGER_T5_TRGO);
    dac_trigger_enable(DAC0, DAC_OUT0);
    dac_wave_mode_config(DAC0, DAC_OUT0, DAC_WAVE_DISABLE);
    dac_enable(DAC0, DAC_OUT0);

    tmr5_cfg();
}

/* ---- RTC ---- */

static void rtc_clk_cfg(void)
{
#if defined(RTC_SRC_LXTAL)
    rcu_osci_on(RCU_LXTAL);
    rcu_osci_stab_wait(RCU_LXTAL);
    rcu_rtc_clock_config(RCU_RTCSRC_LXTAL);
    prescaler_s = 0xFF;
    prescaler_a = 0x7F;
#elif defined(RTC_SRC_IRC32K)
    rcu_osci_on(RCU_IRC32K);
    rcu_osci_stab_wait(RCU_IRC32K);
    rcu_rtc_clock_config(RCU_RTCSRC_IRC32K);
    prescaler_s = 0x13F;
    prescaler_a = 0x63;
#else
#error "RTC时钟源没定义"
#endif
    rcu_periph_clock_enable(RCU_RTC);
    rtc_register_sync_wait();
}

static int rtc_set_time(void)
{
    rtc_initpara.factor_asyn   = prescaler_a;
    rtc_initpara.factor_syn    = prescaler_s;
    rtc_initpara.year           = 0x25;
    rtc_initpara.day_of_week    = RTC_SATURDAY;
    rtc_initpara.month          = RTC_APR;
    rtc_initpara.date           = 0x30;
    rtc_initpara.display_format = RTC_24HOUR;
    rtc_initpara.am_pm          = RTC_AM;
    rtc_initpara.hour    = 0x23;
    rtc_initpara.minute  = 0x59;
    rtc_initpara.second  = 0x50;

    if(ERROR == rtc_init(&rtc_initpara)) return -1;
    RTC_BKP0 = BKP_MAGIC;
    return 0;
}

int bsp_rtc_init(void)
{
    int ret;
    rcu_periph_clock_enable(RCU_PMU);
    pmu_backup_write_enable();
    rtc_clk_cfg();
    RTCSRC_FLAG = GET_BITS(RCU_BDCTL, 8, 9);
    ret = rtc_set_time();
    rcu_all_reset_flag_clear();
    return ret;
}

/* 配置RTC闹钟：seconds_later秒后触发 */
void bsp_rtc_alarm_set(uint32_t seconds_later)
{
    rtc_parameter_struct current_time;
    rtc_alarm_struct alarm_cfg;
    uint32_t total_sec, alarm_sec;
    uint8_t h, m, s;

    /* 读取当前时间 */
    rtc_current_time_get(&current_time);

    /* BCD转十进制 */
    h = ((current_time.hour >> 4) & 0x0F) * 10 + (current_time.hour & 0x0F);
    m = ((current_time.minute >> 4) & 0x0F) * 10 + (current_time.minute & 0x0F);
    s = ((current_time.second >> 4) & 0x0F) * 10 + (current_time.second & 0x0F);

    /* 计算闹钟时间 */
    total_sec = h * 3600 + m * 60 + s + seconds_later;
    total_sec %= 86400;  /* 24小时循环 */

    alarm_sec = total_sec;
    h = alarm_sec / 3600;
    m = (alarm_sec % 3600) / 60;
    s = alarm_sec % 60;

    /* 十进制转BCD */
    alarm_cfg.alarm_hour = ((h / 10) << 4) | (h % 10);
    alarm_cfg.alarm_minute = ((m / 10) << 4) | (m % 10);
    alarm_cfg.alarm_second = ((s / 10) << 4) | (s % 10);

    alarm_cfg.alarm_day = 0x01;
    alarm_cfg.weekday_or_date = RTC_ALARM_DATE_SELECTED;
    alarm_cfg.alarm_mask = RTC_ALARM_DATE_MASK;  /* 只匹配时分秒 */
    alarm_cfg.am_pm = RTC_AM;

    /* 禁用闹钟A */
    rtc_alarm_disable(RTC_ALARM0);

    /* 配置闹钟 */
    rtc_alarm_config(RTC_ALARM0, &alarm_cfg);

    /* 使能闹钟中断 */
    rtc_interrupt_enable(RTC_INT_ALARM0);
    exti_interrupt_flag_clear(EXTI_17);
    exti_init(EXTI_17, EXTI_INTERRUPT, EXTI_TRIG_RISING);

    nvic_irq_enable(RTC_Alarm_IRQn, 0, 0);

    /* 使能闹钟 */
    rtc_alarm_enable(RTC_ALARM0);
}

/* 进入Stop模式，等待RTC闹钟唤醒 */
void bsp_enter_stop_mode(void)
{
    /* 使能PWR时钟 */
    rcu_periph_clock_enable(RCU_PMU);

    /* 配置Stop模式：低功耗，电压调节器低功耗模式，正常驱动 */
    pmu_to_deepsleepmode(PMU_LDO_LOWPOWER, PMU_LOWDRIVER_DISABLE, WFI_CMD);
}
