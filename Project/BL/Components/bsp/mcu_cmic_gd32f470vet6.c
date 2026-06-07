/*
 * GD32F470VET6 板级支持包
 * BootLoader专用初始化代码
 */
#include "mcu_cmic_gd32f470vet6.h"

/* USART0 DMA接收缓冲区 */
uint8_t dbg_rx_buf[512];
uint8_t oled_cmd_buf[2]  = {0x00, 0x00};
uint8_t oled_data_buf[2] = {0x40, 0x00};

/*
 * USART0初始化配置
 * 波特率115200，DMA接收，空闲中断
 */
void bsp_usart0_init(void)
{
    dma_single_data_parameter_struct dma_cfg;

    /* 使能DMA1时钟 */
    rcu_periph_clock_enable(RCU_DMA1);

    /* 配置DMA通道 */
    dma_deinit(USART0_RX_DMA_PERIPH, USART0_RX_DMA_CHANNEL);
    dma_cfg.direction = DMA_PERIPH_TO_MEMORY;
    dma_cfg.memory0_addr = (uint32_t)dbg_rx_buf;
    dma_cfg.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_cfg.number = sizeof(dbg_rx_buf);
    dma_cfg.periph_addr = USART0_RDATA_ADDRESS;
    dma_cfg.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_cfg.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_cfg.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(USART0_RX_DMA_PERIPH, USART0_RX_DMA_CHANNEL, &dma_cfg);

    dma_circulation_disable(USART0_RX_DMA_PERIPH, USART0_RX_DMA_CHANNEL);
    dma_channel_subperipheral_select(USART0_RX_DMA_PERIPH, USART0_RX_DMA_CHANNEL, USART0_RX_DMA_SUBPERI);
    dma_channel_enable(USART0_RX_DMA_PERIPH, USART0_RX_DMA_CHANNEL);

    /* 使能GPIOA时钟 */
    rcu_periph_clock_enable(USART0_CLK_PORT);

    /* 使能USART0时钟 */
    rcu_periph_clock_enable(RCU_USART0);

    /* 配置PA9为TX复用功能 */
    gpio_af_set(USART0_PORT, AF_USART0, USART0_TX);

    /* 配置PA10为RX复用功能 */
    gpio_af_set(USART0_PORT, AF_USART0, USART0_RX);

    /* TX引脚配置 */
    gpio_mode_set(USART0_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART0_TX);
    gpio_output_options_set(USART0_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART0_TX);

    /* RX引脚配置 */
    gpio_mode_set(USART0_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART0_RX);
    gpio_output_options_set(USART0_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART0_RX);

    /* USART参数配置 */
    usart_deinit(USART0);
    usart_baudrate_set(USART0, 115200U);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    usart_dma_receive_config(USART0, USART_RECEIVE_DMA_ENABLE);
    usart_enable(USART0);

    /* 配置NVIC中断 */
    nvic_irq_enable(USART0_IRQn, 0, 0);

    /* 使能空闲中断 */
    usart_interrupt_enable(USART0, USART_INT_IDLE);
}

/*
 * OLED I2C0初始化配置
 */
void bsp_oled_init(void)
{
    dma_single_data_parameter_struct dma_cfg;

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
    dma_single_data_para_struct_init(&dma_cfg);
    dma_cfg.direction = DMA_MEMORY_TO_PERIPH;
    dma_cfg.memory0_addr = (uint32_t)oled_data_buf;
    dma_cfg.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_cfg.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_cfg.number = 2;
    dma_cfg.periph_addr = I2C_DATA_REG;
    dma_cfg.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_cfg.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(DMA0, DMA_CH6, &dma_cfg);
    dma_circulation_disable(DMA0, DMA_CH6);
    dma_channel_subperipheral_select(DMA0, DMA_CH6, DMA_SUBPERI1);
}

/*
 * USART1初始化配置
 * RS485扩展串口，波特率115200，PE8方向控制
 */
void bsp_usart1_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOE);
    rcu_periph_clock_enable(RCU_USART1);

    gpio_af_set(USART1_TX_PORT, AF_USART1, USART1_TX_PIN);
    gpio_af_set(USART1_RX_PORT, AF_USART1, USART1_RX_PIN);

    gpio_mode_set(USART1_TX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART1_TX_PIN);
    gpio_output_options_set(USART1_TX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART1_TX_PIN);
    gpio_mode_set(USART1_RX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART1_RX_PIN);
    gpio_output_options_set(USART1_RX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART1_RX_PIN);

    gpio_mode_set(USART1_DIR_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, USART1_DIR_PIN);
    gpio_output_options_set(USART1_DIR_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART1_DIR_PIN);
    gpio_bit_reset(USART1_DIR_PORT, USART1_DIR_PIN);

    usart_deinit(USART1);
    usart_baudrate_set(USART1, 115200U);
    usart_receive_config(USART1, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART1, USART_TRANSMIT_ENABLE);
    usart_enable(USART1);
}

void bsp_usart_init(void)
{
    bsp_usart0_init();
    bsp_usart1_init();
}
