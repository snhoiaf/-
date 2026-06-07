/*!
    \file    gd32f4xx_it.c
    \brief   interrupt service routines

    \version 2024-12-20, V3.3.1, firmware for GD32F4xx
*/

/*
    Copyright (c) 2024, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#include "gd32f4xx_it.h"
#include "main.h"
#include "mcu_cmic_gd32f470vet6.h"
#include "systick.h"
#include "sdio_sdcard.h"
#include "string.h"
#include "usart_app.h"

extern uint8_t rxbuffer[OTA_UART_RXBUF_SIZE];
extern uint8_t debug_rxbuffer[DEBUG_UART_RXBUF_SIZE];
extern uint8_t usart1_rxbuffer[256];
extern uint8_t uart_dma_buffer[OTA_UART_RXBUF_SIZE];
extern uint8_t debug_uart_dma_buffer[DEBUG_UART_RXBUF_SIZE];
extern uint8_t usart1_uart_dma_buffer[256];
extern __IO uint8_t rx_flag;
extern __IO uint16_t uart_dma_len;
extern __IO uint8_t debug_rx_flag;
extern __IO uint16_t debug_uart_dma_len;
extern __IO uint8_t usart1_rx_flag;
extern __IO uint16_t usart1_uart_dma_len;

/*!
    \brief      this function handles NMI exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void NMI_Handler(void)
{
    /* if NMI exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles HardFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void HardFault_Handler(void)
{
    /* if Hard Fault exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles MemManage exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void MemManage_Handler(void)
{
    /* if Memory Manage exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles BusFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void BusFault_Handler(void)
{
    /* if Bus Fault exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles UsageFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void UsageFault_Handler(void)
{
    /* if Usage Fault exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles SVC exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void SVC_Handler(void)
{
    /* if SVC exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles DebugMon exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void DebugMon_Handler(void)
{
    /* if DebugMon exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles PendSV exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void PendSV_Handler(void)
{
    /* if PendSV exception occurs, go to infinite loop */
    while(1) {
    }
}

static void ota_uart_irq_process(void)
{
    if(RESET != usart_interrupt_flag_get(OTA_UART_PERIPH, USART_INT_FLAG_IDLE)){
        /* clear IDLE flag */
        usart_data_receive(OTA_UART_PERIPH);
    }
}

#if (OTA_UART_IRQN != USART0_IRQn)
void USART0_IRQHandler(void)
{
    uint32_t rx_len;

    if(RESET != usart_interrupt_flag_get(USART0, USART_INT_FLAG_IDLE)) {
        /* clear IDLE flag */
        usart_data_receive(USART0);

        rx_len = DEBUG_UART_RXBUF_SIZE - dma_transfer_number_get(DEBUG_UART_RX_DMA, DEBUG_UART_RX_DMA_CH);
        if((rx_len > 0U) && (rx_len <= DEBUG_UART_RXBUF_SIZE) && (debug_rx_flag == 0U)) {
            memcpy(debug_uart_dma_buffer, debug_rxbuffer, rx_len);
            debug_uart_dma_len = (uint16_t)rx_len;
            debug_rx_flag = 1U;
        }

        memset(debug_rxbuffer, 0, DEBUG_UART_RXBUF_SIZE);
        dma_channel_disable(DEBUG_UART_RX_DMA, DEBUG_UART_RX_DMA_CH);
        dma_flag_clear(DEBUG_UART_RX_DMA, DEBUG_UART_RX_DMA_CH, DMA_FLAG_FTF);
        dma_transfer_number_config(DEBUG_UART_RX_DMA, DEBUG_UART_RX_DMA_CH, DEBUG_UART_RXBUF_SIZE);
        dma_channel_enable(DEBUG_UART_RX_DMA, DEBUG_UART_RX_DMA_CH);
    }
}
#endif

void USART1_IRQHandler(void)
{
    uint32_t rx_len;

    if(RESET != usart_interrupt_flag_get(USART1, USART_INT_FLAG_IDLE)) {
        /* clear IDLE flag */
        usart_data_receive(USART1);

        rx_len = 256 - dma_transfer_number_get(U1_DMA, U1_DMA_CH);
        if((rx_len > 0U) && (rx_len <= 256) && (usart1_rx_flag == 0U)) {
            memcpy(usart1_uart_dma_buffer, usart1_rxbuffer, rx_len);
            usart1_uart_dma_len = (uint16_t)rx_len;
            usart1_rx_flag = 1U;
        }

        memset(usart1_rxbuffer, 0, 256);
        dma_channel_disable(U1_DMA, U1_DMA_CH);
        dma_flag_clear(U1_DMA, U1_DMA_CH, DMA_FLAG_FTF);
        dma_transfer_number_config(U1_DMA, U1_DMA_CH, 256);
        dma_channel_enable(U1_DMA, U1_DMA_CH);
    }
}



void USART2_IRQHandler(void)
{
    ota_uart_irq_process();
}

void SDIO_IRQHandler(void)
{
    sd_interrupts_process();
}

/*!
    \brief    this function handles SysTick exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void SysTick_Handler(void)
{
    delay_decrement();
}

/*!
    \brief      RTC Alarm interrupt handler
    \param[in]  none
    \param[out] none
    \retval     none
*/
extern volatile uint8_t rtc_alarm_wakeup_flag;

void RTC_Alarm_IRQHandler(void)
{
    if(RESET != rtc_flag_get(RTC_FLAG_ALRM0)){
        rtc_flag_clear(RTC_FLAG_ALRM0);
        exti_interrupt_flag_clear(EXTI_17);
        rtc_alarm_wakeup_flag = 1;
    }
}
