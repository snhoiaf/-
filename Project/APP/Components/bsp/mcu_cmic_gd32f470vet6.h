/*
 * V2板 BSP 引脚定义 + 初始化函数声明
 * 2026-05 重写，引脚不变，组织方式变了
 */
#ifndef __BSP_F470_H
#define __BSP_F470_H

#include "gd32f4xx.h"
#include "gd32f4xx_sdio.h"
#include "gd32f4xx_dma.h"
#include "systick.h"
#include "ebtn.h"
#include "oled.h"
#include "gd25qxx.h"
#include "gd30ad3344.h"
#include "sdio_sdcard.h"
#include "ff.h"
#include "diskio.h"
#include "sd_app.h"
#include "led_app.h"
#include "adc_app.h"
#include "oled_app.h"
#include "usart_app.h"
#include "rtc_app.h"
#include "btn_app.h"
#include "scheduler.h"
#include "ota_uart.h"
#include "perf_counter.h"

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================
 * SDIO 引脚
 *============================================*/
#define SD_CD_PORT   GPIOE
#define SD_CD_PIN    GPIO_PIN_2
#define SD_CMD_PORT  GPIOD
#define SD_CMD_PIN   GPIO_PIN_2
#define SD_CLK_PORT  GPIOC
#define SD_CLK_PIN   GPIO_PIN_12
#define SD_D3_PORT   GPIOC
#define SD_D3_PIN    GPIO_PIN_11
#define SD_D2_PORT   GPIOC
#define SD_D2_PIN    GPIO_PIN_10
#define SD_D1_PORT   GPIOC
#define SD_D1_PIN    GPIO_PIN_9
#define SD_D0_PORT   GPIOC
#define SD_D0_PIN    GPIO_PIN_8

/*=============================================
 * ADC 引脚
 *============================================*/
#define ADC_CH10_PORT  GPIOC
#define ADC_CH10_PIN   GPIO_PIN_0
#define ADC_CH12_PORT  GPIOC
#define ADC_CH12_PIN   GPIO_PIN_2

/*=============================================
 * ETH RMII 引脚
 *============================================*/
#define ETH_MDC_PORT     GPIOC
#define ETH_MDC_PIN      GPIO_PIN_1
#define ETH_REFCLK_PORT  GPIOA
#define ETH_REFCLK_PIN   GPIO_PIN_1
#define ETH_MDIO_PORT    GPIOA
#define ETH_MDIO_PIN     GPIO_PIN_2
#define ETH_CRSDV_PORT   GPIOA
#define ETH_CRSDV_PIN    GPIO_PIN_7
#define ETH_RXD0_PORT    GPIOC
#define ETH_RXD0_PIN     GPIO_PIN_4
#define ETH_RXD1_PORT    GPIOC
#define ETH_RXD1_PIN     GPIO_PIN_5
#define ETH_TXEN_PORT    GPIOB
#define ETH_TXEN_PIN     GPIO_PIN_11
#define ETH_TXD0_PORT    GPIOB
#define ETH_TXD0_PIN     GPIO_PIN_12
#define ETH_TXD1_PORT    GPIOB
#define ETH_TXD1_PIN     GPIO_PIN_13

/*=============================================
 * 控制引脚
 *============================================*/
#define WKUP_PORT    GPIOA
#define WKUP_PIN     GPIO_PIN_0
#define WRST_PORT    GPIOA
#define WRST_PIN     GPIO_PIN_3
#define PHY_RST_PORT GPIOD
#define PHY_RST_PIN  GPIO_PIN_3

/*=============================================
 * DAC 引脚
 *============================================*/
#define DAC0_PORT  GPIOA
#define DAC0_PIN   GPIO_PIN_4
#define DAC1_PORT  GPIOA
#define DAC1_PIN   GPIO_PIN_5

/*=============================================
 * CS / 方向控制引脚
 *============================================*/
#define RS485_DIR_PORT  GPIOE
#define RS485_DIR_PIN   GPIO_PIN_8   /* 1=发, 0=收 */
#define W_CS_PORT       GPIOE
#define W_CS_PIN        GPIO_PIN_9
#define S_CS_PORT       GPIOE
#define S_CS_PIN        GPIO_PIN_10

/*=============================================
 * SPI3 引脚 (接GD30AD3344)
 *============================================*/
#define SPI3_SCK_PORT   GPIOE
#define SPI3_SCK_PIN    GPIO_PIN_12
#define SPI3_MISO_PORT  GPIOE
#define SPI3_MISO_PIN   GPIO_PIN_13
#define SPI3_MOSI_PORT  GPIOE
#define SPI3_MOSI_PIN   GPIO_PIN_14

/*=============================================
 * OLED I2C0 引脚
 *============================================*/
#define OLED_SDA_PORT  GPIOB
#define OLED_SDA_PIN   GPIO_PIN_9
#define OLED_SCL_PORT  GPIOB
#define OLED_SCL_PIN   GPIO_PIN_8

/*=============================================
 * SPI Flash SPI0 引脚
 *============================================*/
#define FL_SCK_PORT   GPIOB
#define FL_SCK_PIN    GPIO_PIN_3
#define FL_MISO_PORT  GPIOB
#define FL_MISO_PIN   GPIO_PIN_4
#define FL_MOSI_PORT  GPIOB
#define FL_MOSI_PIN   GPIO_PIN_5
#define FL_CS_PORT    GPIOA
#define FL_CS_PIN     GPIO_PIN_15

/*=============================================
 * 复用功能 AF
 *============================================*/
#define AF_I2C0      GPIO_AF_4
#define AF_SPI0      GPIO_AF_5
#define AF_SPI3      GPIO_AF_5
#define AF_USART0    GPIO_AF_7
#define AF_USART1    GPIO_AF_7
#define AF_USART2    GPIO_AF_7
#define AF_USART5    GPIO_AF_8
#define AF_ETH       GPIO_AF_11
#define AF_SDIO      GPIO_AF_12

/*=============================================
 * LED 引脚 (GPIOD, 高电平亮)
 *============================================*/
#define LED_PORT     GPIOD
#define LED_CLK      RCU_GPIOD
#define LED1         GPIO_PIN_10
#define LED2         GPIO_PIN_11
#define LED3         GPIO_PIN_12
#define LED4         GPIO_PIN_13
#define LED5         GPIO_PIN_14
#define LED6         GPIO_PIN_15

#define LED_LVL  1  /* 1=高有效, 0=低有效 */

#if LED_LVL
#define LED_SET(pin, on) do{ if(on) GPIO_BOP(LED_PORT)=(pin); else GPIO_BC(LED_PORT)=(pin); }while(0)
#else
#define LED_SET(pin, on) do{ if(on) GPIO_BC(LED_PORT)=(pin); else GPIO_BOP(LED_PORT)=(pin); }while(0)
#endif

#define LED1_SET(x)  LED_SET(LED1, (x))
#define LED2_SET(x)  LED_SET(LED2, (x))
#define LED3_SET(x)  LED_SET(LED3, (x))
#define LED4_SET(x)  LED_SET(LED4, (x))
#define LED5_SET(x)  LED_SET(LED5, (x))
#define LED6_SET(x)  LED_SET(LED6, (x))

#define LED1_TG  do{ GPIO_TG(LED_PORT)=LED1; }while(0)
#define LED2_TG  do{ GPIO_TG(LED_PORT)=LED2; }while(0)
#define LED3_TG  do{ GPIO_TG(LED_PORT)=LED3; }while(0)
#define LED4_TG  do{ GPIO_TG(LED_PORT)=LED4; }while(0)
#define LED5_TG  do{ GPIO_TG(LED_PORT)=LED5; }while(0)
#define LED6_TG  do{ GPIO_TG(LED_PORT)=LED6; }while(0)

#define LED1_ON   do{ LED_SET(LED1, 1); }while(0)
#define LED2_ON   do{ LED_SET(LED2, 1); }while(0)
#define LED3_ON   do{ LED_SET(LED3, 1); }while(0)
#define LED4_ON   do{ LED_SET(LED4, 1); }while(0)
#define LED5_ON   do{ LED_SET(LED5, 1); }while(0)
#define LED6_ON   do{ LED_SET(LED6, 1); }while(0)

#define LED1_OFF  do{ LED_SET(LED1, 0); }while(0)
#define LED2_OFF  do{ LED_SET(LED2, 0); }while(0)
#define LED3_OFF  do{ LED_SET(LED3, 0); }while(0)
#define LED4_OFF  do{ LED_SET(LED4, 0); }while(0)
#define LED5_OFF  do{ LED_SET(LED5, 0); }while(0)
#define LED6_OFF  do{ LED_SET(LED6, 0); }while(0)

/* 兼容原模板的LED_TOGGLE宏 */
#define LED1_TOGGLE  LED1_TG
#define LED2_TOGGLE  LED2_TG
#define LED3_TOGGLE  LED3_TG
#define LED4_TOGGLE  LED4_TG
#define LED5_TOGGLE  LED5_TG
#define LED6_TOGGLE  LED6_TG

void bsp_led_init(void);

/*=============================================
 * 按键引脚 (上拉输入，低有效)
 *============================================*/
#define KE_PORT   GPIOE
#define KE_CLK    RCU_GPIOE
#define KB_PORT   GPIOB
#define KB_CLK    RCU_GPIOB
#define KA_PORT   GPIOA
#define KA_CLK    RCU_GPIOA

#define K1_PIN    GPIO_PIN_15
#define K2_PIN    GPIO_PIN_6
#define K3_PIN    GPIO_PIN_11
#define K4_PIN    GPIO_PIN_4
#define K5_PIN    GPIO_PIN_7
#define K6_PIN    GPIO_PIN_6
#define KW_PIN    GPIO_PIN_0

#define KEY1_READ  gpio_input_bit_get(KE_PORT, K1_PIN)
#define KEY2_READ  gpio_input_bit_get(KE_PORT, K2_PIN)
#define KEY3_READ  gpio_input_bit_get(KE_PORT, K3_PIN)
#define KEY4_READ  gpio_input_bit_get(KE_PORT, K4_PIN)
#define KEY5_READ  gpio_input_bit_get(KE_PORT, K5_PIN)
#define KEY6_READ  gpio_input_bit_get(KB_PORT, K6_PIN)
#define KEYW_READ  gpio_input_bit_get(KA_PORT, KW_PIN)

void bsp_btn_init(void);

/*=============================================
 * OLED
 *============================================*/
#define I2C_OWN_ADDR   0x72
#define I2C_SLV_ADDR   0x82
#define I2C_DATA_REG   ((uint32_t)&I2C_DATA(I2C0))

void bsp_oled_init(void);

/*=============================================
 * SPI Flash
 *============================================*/
#define FL_SPI       SPI0
#define FL_SPI_CLK   RCU_SPI0
#define FL_GPIO_PORT FL_SCK_PORT
#define FL_GPIO_CLK  RCU_GPIOB
#define FL_CS_CLK    RCU_GPIOA

void bsp_gd25qxx_init(void);

/*=============================================
 * GD30AD3344
 *============================================*/
#define AD_SPI       SPI3
#define AD_SPI_CLK   RCU_SPI3
#define AD_GPIO_PORT SPI3_SCK_PORT
#define AD_GPIO_CLK  RCU_GPIOE

void bsp_gd30ad3344_init(void);

/*=============================================
 * 串口配置
 *============================================*/
#define DEBUG_USART  USART0

/* DMA地址宏 */
#define USART0_DR    ((uint32_t)&USART_DATA(USART0))
#define USART1_DR    ((uint32_t)&USART_DATA(USART1))
#define USART2_DR    ((uint32_t)&USART_DATA(USART2))
#define USART5_DR    ((uint32_t)&USART_DATA(USART5))

/* Debug串口 DMA */
#define DBG_DMA       DMA1
#define DBG_DMA_CH    DMA_CH5
#define DBG_DMA_SUB   DMA_SUBPERI4
#define DBG_DMA_RCU   RCU_DMA1
#define DBG_RX_SZ     512U
#define DBG_DR        USART0_DR

/* OTA串口 DMA */
#define OTA_UART      USART2
#define OTA_UART_RCU  RCU_USART2
#define OTA_IRQn      39
#define OTA_PORT      GPIOD
#define OTA_PORT_RCU  RCU_GPIOD
#define OTA_TX        GPIO_PIN_8
#define OTA_RX        GPIO_PIN_9
#define OTA_AF        AF_USART2
#define OTA_DMA       DMA0
#define OTA_DMA_CH    DMA_CH1
#define OTA_DMA_SUB   DMA_SUBPERI4
#define OTA_DMA_RCU   RCU_DMA0
#define OTA_BAUD      921600U
#define OTA_RX_SZ     2048U
#define OTA_DR        USART2_DR

/* USART1 DMA */
#define U1_DMA        DMA0
#define U1_DMA_CH     DMA_CH5
#define U1_DMA_SUB    DMA_SUBPERI4
#define U1_DR         USART1_DR

/* USART5 DMA */
#define U5_DMA        DMA1
#define U5_DMA_CH     DMA_CH1
#define U5_DMA_SUB    DMA_SUBPERI5
#define U5_DR         USART5_DR

void bsp_usart0_init(void);
void bsp_usart1_init(void);
void bsp_usart2_init(void);
void bsp_usart5_init(void);
void bsp_usart_init(void);
void bsp_usart_all_init(void);
void bsp_ota_dma_rearm(void);
uint32_t bsp_ota_dma_cnt(void);

/*=============================================
 * ADC
 *============================================*/
#define ADC_GPIO_PORT  ADC_CH10_PORT
#define ADC_GPIO_CLK   RCU_GPIOC

void bsp_adc_init(void);

/*=============================================
 * DAC
 *============================================*/
#define DAC_N          1
#define DAC_R12DH      0x40007408UL
#define DAC_GPIO_PORT  DAC0_PORT
#define DAC_GPIO_CLK   RCU_GPIOA

void bsp_dac_init(void);

/*=============================================
 * RTC
 *============================================*/
#define RTC_SRC_LXTAL
#define BKP_MAGIC  0x32F0

int bsp_rtc_init(void);

/*=============================================
 * 兼容旧宏名（中断/usart_app/gd30ad3344等引用）
 *============================================*/
/* USART0 */
#define USART0_TX_PORT  GPIOA
#define USART0_TX_PIN   GPIO_PIN_9
#define USART0_RX_PIN   GPIO_PIN_10
#define USART0_TX       USART0_TX_PIN
#define USART0_RX       USART0_RX_PIN

/* USART1 */
#define USART1_TX_PORT  GPIOD
#define USART1_TX_PIN   GPIO_PIN_5
#define USART1_RX_PIN   GPIO_PIN_6

/* USART5 */
#define USART5_TX_PORT  GPIOC
#define USART5_TX_PIN   GPIO_PIN_6
#define USART5_RX_PIN   GPIO_PIN_7

/* SPI3 旧名 */
#define SPI3_PORT  SPI3_SCK_PORT
#define SPI3_NSS   S_CS_PIN
#define OTA_UART_PERIPH         OTA_UART
#define OTA_UART_RXBUF_SIZE     OTA_RX_SZ
#define OTA_UART_DMA            OTA_DMA
#define OTA_UART_DMA_CH         OTA_DMA_CH
#define OTA_UART_RDATA_ADDRESS  OTA_DR

#define DEBUG_UART_RX_DMA       DBG_DMA
#define DEBUG_UART_RX_DMA_RCU   DBG_DMA_RCU
#define DEBUG_UART_RX_DMA_CH    DBG_DMA_CH
#define DEBUG_UART_RXBUF_SIZE   DBG_RX_SZ
#define DEBUG_UART_RDATA_ADDRESS DBG_DR

#define USART0_RDATA_ADDRESS    USART0_DR
#define USART1_RDATA_ADDRESS    USART1_DR
#define USART2_RDATA_ADDRESS    USART2_DR
#define USART5_RDATA_ADDRESS    USART5_DR

#define CONVERT_NUM             DAC_N
#define I2C0_OWN_ADDRESS7       I2C_OWN_ADDR
#define I2C0_SLAVE_ADDRESS7     I2C_SLV_ADDR
#define I2C0_DATA_ADDRESS       I2C_DATA_REG

#define RTC_CLOCK_SOURCE_LXTAL
#define BKP_VALUE               BKP_MAGIC

#ifdef __cplusplus
}
#endif

#endif
