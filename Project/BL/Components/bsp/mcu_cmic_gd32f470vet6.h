/*
 * GD32F470VET6 板级硬件定义
 * BootLoader专用引脚映射和外设配置
 */
#ifndef MCU_CMIC_GD32F470VET6_H
#define MCU_CMIC_GD32F470VET6_H

#include "gd32f4xx.h"
#include "gd32f4xx_dma.h"
#include "gd32f4xx_i2c.h"
#include "gd32f4xx_gpio.h"
#include "systick.h"
#include "usart_app.h"
#include "oled.h"

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* OLED I2C0引脚定义 */
#define OLED_SDA_PORT            GPIOB
#define OLED_SDA_PIN             GPIO_PIN_9
#define OLED_SCL_PORT            GPIOB
#define OLED_SCL_PIN             GPIO_PIN_8
#define AF_I2C0                  GPIO_AF_4
#define I2C_OWN_ADDR             0x72
#define I2C_DATA_REG             ((uint32_t)&I2C_DATA(I2C0))

#define USART0_RX_PORT            GPIOA
#define USART0_RX_PIN             GPIO_PIN_10
#define USART0_TX_PORT            GPIOA
#define USART0_TX_PIN             GPIO_PIN_9

#define USART1_RX_PORT            GPIOD
#define USART1_RX_PIN             GPIO_PIN_6
#define USART1_TX_PORT            GPIOD
#define USART1_TX_PIN             GPIO_PIN_5
#define USART1_DIR_PORT           GPIOE
#define USART1_DIR_PIN            GPIO_PIN_8

/* USART复用功能 */
#define AF_USART0                 GPIO_AF_7
#define AF_USART1                 GPIO_AF_7

/* 调试串口定义 */
#define DEBUG_USART               (USART0)
#define USART0_RDATA_ADDRESS      ((uint32_t)&USART_DATA(USART0))

/* USART0 DMA配置 */
#define USART0_RX_DMA_PERIPH      DMA1
#define USART0_RX_DMA_CHANNEL     DMA_CH5
#define USART0_RX_DMA_SUBPERI     DMA_SUBPERI4

/* USART1 DMA配置 (DMA0 CH5 SUB4) */
#define USART1_RX_DMA_PERIPH      DMA0
#define USART1_RX_DMA_CHANNEL     DMA_CH5
#define USART1_RX_DMA_SUBPERI     DMA_SUBPERI4
#define USART1_RDATA_ADDRESS      ((uint32_t)&USART_DATA(USART1))
#define USART1_RX_BUF_SIZE        2048U

/* 串口端口别名 */
#define USART0_PORT               USART0_TX_PORT
#define USART0_CLK_PORT           RCU_GPIOA
#define USART0_TX                 USART0_TX_PIN
#define USART0_RX                 USART0_RX_PIN

/* 兼容性别名定义 */
#define USART_PORT                USART0_PORT
#define USARTI_CLK_PORT           USART0_CLK_PORT
#define USART_TX                  USART0_TX
#define USART_RX                  USART0_RX

/* 函数声明 */
void bsp_usart0_init(void);
void bsp_usart1_init(void);
void bsp_usart_init(void);
void bsp_oled_init(void);

#ifdef __cplusplus
  }
#endif

#endif /* MCU_CMIC_GD32F470VET6_H */
