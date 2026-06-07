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
#include "string.h"

extern uint8_t dbg_rx_buf[512];
extern uint8_t rx_frame_buf[256];
extern uint8_t uart_rx_ready;

extern uint8_t usart1_rx_buf[USART1_RX_BUF_SIZE];
volatile uint32_t usart1_rx_len = 0;
volatile uint8_t  usart1_rx_flag = 0;

/*!
    \brief      this function handles NMI exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void NMI_Handler(void)
{
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
    while(1) {
    }
}

/*!
    \brief      USART0中断处理函数
    \param[in]  none
    \param[out] none
    \retval     none
*/
void USART0_IRQHandler(void)
{
    uint32_t received;
    uint32_t to_copy;

    if(RESET != usart_interrupt_flag_get(USART0, USART_INT_FLAG_IDLE)){
        /* 清除空闲标志 */
        usart_data_receive(USART0);
        dma_channel_disable(USART0_RX_DMA_PERIPH, USART0_RX_DMA_CHANNEL);

        /* 计算接收数据长度 */
        received = sizeof(dbg_rx_buf) - dma_transfer_number_get(USART0_RX_DMA_PERIPH, USART0_RX_DMA_CHANNEL);
        if((received > 0U) && (received <= sizeof(dbg_rx_buf))){
            to_copy = received;
            if(to_copy >= sizeof(rx_frame_buf)){
                to_copy = sizeof(rx_frame_buf) - 1U;
            }
            memset(rx_frame_buf, 0, sizeof(rx_frame_buf));
            memcpy(rx_frame_buf, dbg_rx_buf, to_copy);
            uart_rx_ready = 1;
        }
        memset(dbg_rx_buf, 0, sizeof(dbg_rx_buf));

        /* 重新配置DMA */
        dma_flag_clear(USART0_RX_DMA_PERIPH, USART0_RX_DMA_CHANNEL, DMA_FLAG_FTF);
        dma_transfer_number_config(USART0_RX_DMA_PERIPH, USART0_RX_DMA_CHANNEL, sizeof(dbg_rx_buf));
        dma_channel_enable(USART0_RX_DMA_PERIPH, USART0_RX_DMA_CHANNEL);
    }
}

void SDIO_IRQHandler(void)
{
}

void USART1_IRQHandler(void)
{
    if(RESET != usart_interrupt_flag_get(USART1, USART_INT_FLAG_IDLE)){
        usart_data_receive(USART1);
        dma_channel_disable(USART1_RX_DMA_PERIPH, USART1_RX_DMA_CHANNEL);

        usart1_rx_len = USART1_RX_BUF_SIZE
                        - dma_transfer_number_get(USART1_RX_DMA_PERIPH, USART1_RX_DMA_CHANNEL);

        if(usart1_rx_len > 0U){
            usart1_rx_flag = 1U;  /* 始终覆盖，保持最新帧 */
        }

        dma_flag_clear(USART1_RX_DMA_PERIPH, USART1_RX_DMA_CHANNEL, DMA_FLAG_FTF);
        dma_transfer_number_config(USART1_RX_DMA_PERIPH, USART1_RX_DMA_CHANNEL, USART1_RX_BUF_SIZE);
        dma_channel_enable(USART1_RX_DMA_PERIPH, USART1_RX_DMA_CHANNEL);
    }
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
