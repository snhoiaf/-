/*!
    \file    gd30ad3344.c
    \brief   gd30ad3344 driver
    
    \version 2024-10-08, V1.0.0, firmware for GD30AD3344
*/

#include "gd30ad3344.h"

extern uint8_t spi3_send_array[ARRAYSIZE];    // SPI3 DMA 发送缓冲区
extern uint8_t spi3_receive_array[ARRAYSIZE]; // SPI3 DMA 接收缓冲区

/**
 * @brief 使用 DMA 发送并接收一个字节
 * @param byte 要发送的字节
 * @return 从 SPI 总线接收到的字节
 */
uint8_t spi_gd30ad3344_send_byte_dma(uint8_t byte)
{
    /* 将数据放入发送缓冲区 */
    spi3_send_array[0] = byte;
    
    /* 配置发送 DMA，只发送一个字节 */
    dma_single_data_parameter_struct dma_init_struct;
    
    /* 配置 DMA 发送通道 */
    dma_deinit(DMA1, DMA_CH4);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(SPI_GD30AD3344);
    dma_init_struct.memory0_addr        = (uint32_t)spi3_send_array;
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_init_struct.number              = 1; /* 只发送一个字节 */
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
    dma_single_data_mode_init(DMA1, DMA_CH4, &dma_init_struct);
    dma_channel_subperipheral_select(DMA1, DMA_CH4, DMA_SUBPERI5);
    
    /* 配置 DMA 接收通道 */
    dma_deinit(DMA1, DMA_CH3);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(SPI_GD30AD3344);
    dma_init_struct.memory0_addr        = (uint32_t)spi3_receive_array;
    dma_init_struct.direction           = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(DMA1, DMA_CH3, &dma_init_struct);
    dma_channel_subperipheral_select(DMA1, DMA_CH3, DMA_SUBPERI5);
    
    /* 启用接收和发送的 DMA 通道 */
    dma_channel_enable(DMA1, DMA_CH3);
    dma_channel_enable(DMA1, DMA_CH4);
    
    /* 启用 SPI 的 DMA 接收和发送功能 */
    spi_dma_enable(SPI_GD30AD3344, SPI_DMA_RECEIVE);
    spi_dma_enable(SPI_GD30AD3344, SPI_DMA_TRANSMIT);
    
    /* 等待 DMA 传输完成 */
    while(RESET == dma_flag_get(DMA1, DMA_CH3, DMA_FLAG_FTF));
    
    /* 禁用 DMA */
    spi_dma_disable(SPI_GD30AD3344, SPI_DMA_RECEIVE);
    spi_dma_disable(SPI_GD30AD3344, SPI_DMA_TRANSMIT);
    dma_channel_disable(DMA1, DMA_CH3);
    dma_channel_disable(DMA1, DMA_CH4);
    
    /* 清除 DMA 标志 */
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_FTF);
    dma_flag_clear(DMA1, DMA_CH4, DMA_FLAG_FTF);
    
    /* 返回接收到的数据 */
    return spi3_receive_array[0];
}

/**
 * @brief 使用 DMA 发送并接收一个半字（16位数据）
 * @param half_word 要发送的半字
 * @return 从 SPI 总线接收到的半字
 */
uint16_t spi_gd30ad3344_send_halfword_dma(uint16_t half_word)
{
    SPI_GD30AD3344_CS_LOW();
    uint16_t rx_data;
    
    /* 先发送高8位 */
    spi3_send_array[0] = (uint8_t)(half_word >> 8);
    spi3_send_array[1] = (uint8_t)half_word;
    
    /* 配置 DMA 参数 */
    dma_single_data_parameter_struct dma_init_struct;
    
    /* 配置 DMA 发送通道 */
    dma_deinit(DMA1, DMA_CH4);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(SPI_GD30AD3344);
    dma_init_struct.memory0_addr        = (uint32_t)spi3_send_array;
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_init_struct.number              = 2; /* 发送2个字节 */
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
    dma_single_data_mode_init(DMA1, DMA_CH4, &dma_init_struct);
    dma_channel_subperipheral_select(DMA1, DMA_CH4, DMA_SUBPERI5);
    
    /* 配置 DMA 接收通道 */
    dma_deinit(DMA1, DMA_CH3);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(SPI_GD30AD3344);
    dma_init_struct.memory0_addr        = (uint32_t)spi3_receive_array;
    dma_init_struct.direction           = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(DMA1, DMA_CH3, &dma_init_struct);
    dma_channel_subperipheral_select(DMA1, DMA_CH3, DMA_SUBPERI5);
    
    /* 启用接收和发送的 DMA 通道 */
    dma_channel_enable(DMA1, DMA_CH3);
    dma_channel_enable(DMA1, DMA_CH4);
    
    /* 启用 SPI 的 DMA 接收和发送功能 */
    spi_dma_enable(SPI_GD30AD3344, SPI_DMA_RECEIVE);
    spi_dma_enable(SPI_GD30AD3344, SPI_DMA_TRANSMIT);
    
    /* 等待 DMA 传输完成 */
    while(RESET == dma_flag_get(DMA1, DMA_CH3, DMA_FLAG_FTF));
    
    /* 禁用 DMA */
    spi_dma_disable(SPI_GD30AD3344, SPI_DMA_RECEIVE);
    spi_dma_disable(SPI_GD30AD3344, SPI_DMA_TRANSMIT);
    dma_channel_disable(DMA1, DMA_CH3);
    dma_channel_disable(DMA1, DMA_CH4);
    
    /* 清除 DMA 标志 */
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_FTF);
    dma_flag_clear(DMA1, DMA_CH4, DMA_FLAG_FTF);
    
    /* 组合接收到的数据 */
    rx_data = (uint16_t)(spi3_receive_array[0] << 8);
    rx_data |= spi3_receive_array[1];
    SPI_GD30AD3344_CS_HIGH();
    return rx_data;
}

/**
 * @brief 使用 DMA 发送和接收多个字节
 * @param tx_buffer 发送缓冲区
 * @param rx_buffer 接收缓冲区
 * @param size 传输大小
 */
void spi_gd30ad3344_transmit_receive_dma(uint8_t *tx_buffer, uint8_t *rx_buffer, uint16_t size)
{
    /* 检查传输大小是否超过缓冲区 */
    if (size > ARRAYSIZE) {
        size = ARRAYSIZE;
    }
    
    /* 准备发送数据 */
    for (uint16_t i = 0; i < size; i++) {
        spi3_send_array[i] = tx_buffer[i];
    }
    
    /* 配置 DMA 参数 */
    dma_single_data_parameter_struct dma_init_struct;
    
    /* 配置 DMA 发送通道 */
    dma_deinit(DMA1, DMA_CH4);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(SPI_GD30AD3344);
    dma_init_struct.memory0_addr        = (uint32_t)spi3_send_array;
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_init_struct.number              = size;
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
    dma_single_data_mode_init(DMA1, DMA_CH2, &dma_init_struct);
    dma_channel_subperipheral_select(DMA1, DMA_CH2, DMA_SUBPERI5);
    
    /* 配置 DMA 接收通道 */
    dma_deinit(DMA0, DMA_CH3);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(SPI_GD30AD3344);
    dma_init_struct.memory0_addr        = (uint32_t)spi3_receive_array;
    dma_init_struct.direction           = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(DMA1, DMA_CH3, &dma_init_struct);
    dma_channel_subperipheral_select(DMA1, DMA_CH3, DMA_SUBPERI5);
    
    /* 启用接收和发送的 DMA 通道 */
    dma_channel_enable(DMA1, DMA_CH3);
    dma_channel_enable(DMA1, DMA_CH4);
    
    /* 启用 SPI 的 DMA 接收和发送功能 */
    spi_dma_enable(SPI_GD30AD3344, SPI_DMA_RECEIVE);
    spi_dma_enable(SPI_GD30AD3344, SPI_DMA_TRANSMIT);
    
    /* 等待 DMA 传输完成 */
    while(RESET == dma_flag_get(DMA1, DMA_CH3, DMA_FLAG_FTF));
    
    /* 禁用 DMA */
    spi_dma_disable(SPI_GD30AD3344, SPI_DMA_RECEIVE);
    spi_dma_disable(SPI_GD30AD3344, SPI_DMA_TRANSMIT);
    dma_channel_disable(DMA1, DMA_CH3);
    dma_channel_disable(DMA1, DMA_CH4);
    
    /* 清除 DMA 标志 */
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_FTF);
    dma_flag_clear(DMA1, DMA_CH4, DMA_FLAG_FTF);
    
    /* 复制接收到的数据到接收缓冲区 */
    for (uint16_t i = 0; i < size; i++) {
        rx_buffer[i] = spi3_receive_array[i];
    }
}

/**
 * @brief 等待 DMA 传输完成
 */
void spi_gd30ad3344_wait_for_dma_end(void)
{
    /* 等待 DMA 传输完成 */
    while(RESET == dma_flag_get(DMA1, DMA_CH3, DMA_FLAG_FTF));
    
    /* 清除 DMA 标志 */
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_FTF);
    dma_flag_clear(DMA1, DMA_CH4, DMA_FLAG_FTF);
}


GD30AD3344 GD30AD3344_InitStruct;

void GD30AD3344_Init(void)
{
    GD30AD3344_InitStruct.SS         = 0;        //写状态:0无作用 1开始单次转换（默认） 读的时候总是返回0 
    GD30AD3344_InitStruct.MUX        = 4;        // 0(默认)      1         2         3         4         5         6         7
                                                //AIN0~AIN1 AIN0~AIN3 AIN1~AIN3 AIN2~AIN3 AIN0~GND  AIN1~GND  AIN2~GND  AIN3~GND 
    GD30AD3344_InitStruct.PGA        = 1;       //    0         1       2(默认)     3         4         5         6         7
                                                // ±6.144V   ±4.096V   ±2.048V   ±1.024V   ±0.512V   ±0.256V   ±0.256V  ±0.256V
    GD30AD3344_InitStruct.MODE       = 0;        //0:连续转换模式    1:掉电，单次转换模式（默认） 
    GD30AD3344_InitStruct.DR         = 1;        //    0         1         2         3         4         5         6         7
                                                //  6.25SPS     12.5SPS   25SPS     50SPS     100SPS    250SPS    500SPS    1000SPS
    GD30AD3344_InitStruct.RESERVED_1 = 0;        //保留:写的时候写1，读的时候返回0或1 
    GD30AD3344_InitStruct.PULL_UP_EN = 0;        //0:关闭DOUT引脚上拉电阻(默认)    1:开启DOUT引脚上拉电阻
    GD30AD3344_InitStruct.NOP        = 1;        //0:不更新配置寄存器的数据  1:更新配置寄存器的数据(默认)  2:无效数据，且不更新配置寄存器数据
    GD30AD3344_InitStruct.RESERVED   = 1;        //保留:写的时候写1，读的时候返回0或1 
    
    spi_enable(SPI_GD30AD3344);
    spi_gd30ad3344_send_halfword_dma(GD30AD3344_InitStruct_Value);
    my_printf(DEBUG_USART, "0x%4X", GD30AD3344_InitStruct_Value);
}

float PGA_DATA = 0.0;
float ADS118_PGA_SET(GD30AD3344_PGA_TypeDef PGA)
{

    if(PGA == GD30AD3344_PGA_6V144)
        PGA_DATA = 6.144;
        

    if(PGA == GD30AD3344_PGA_4V096)
        PGA_DATA = 4.096;


    if(PGA == GD30AD3344_PGA_2V048)
        PGA_DATA = 2.048;


    if(PGA == GD30AD3344_PGA_1V024)
        PGA_DATA = 1.024;


    if(PGA == GD30AD3344_PGA_0V512)
        PGA_DATA = 0.512;


    if(PGA == GD30AD3344_PGA_0V512)
        PGA_DATA = 0.256;

    return (float)PGA_DATA;

}


float GD30AD3344_AD_Read(GD30AD3344_Channel_TypeDef CH,GD30AD3344_PGA_TypeDef Ref)          
{
    uint16_t raw_data;
    float result = 0.0;
    GD30AD3344_InitStruct.MUX     =	CH; // 0(默认)      1         2         3         4         5         6         7
    //AIN0~AIN1   AIN0~AIN3   AIN1~AIN3 AIN2~AIN3 AIN0~GND  AIN1~GND  AIN2~GND  AIN3~GND 
    GD30AD3344_InitStruct.PGA     =	Ref; //    0         1       2(默认)     3         4         5         6         7
    // ±6.144V   ±4.096V   ±2.048V   ±1.024V   ±0.512V   ±0.256V   ±0.256V  ±0.256V
    
    
    raw_data = spi_gd30ad3344_send_halfword_dma(GD30AD3344_InitStruct_Value);
    
//    elog_i("drv", "config : %d",  GD30AD3344_InitStruct_Value);
//    my_printf(DEBUG_USART, "%d\r\n", raw_data);
//    if(raw_data & 0x8000) raw_data = (~raw_data) + 1;
    result = (float)raw_data * ADS118_PGA_SET(Ref) / 32768;
    return (float)result;
}

////平均值滤波算法
//float GD30AD3344_correct(GD30AD3344_Channel_TypeDef ch,unsigned char len)
//{
//    uint16_t i;
////    u8 len = 10;

//    float temp_data ;
//    float sampling[len];
//    float num = 0.0;

//        for (i = 0; i < len; i++) 
//        {
//            temp_data = GD30AD3344_AD_Read(ch,GD30AD3344_PGA_2V048);
//            sampling[i] = temp_data;
//        }
//        
//        for(i = 0;i < len;i++)
//        {
//            num += sampling[i];
//        }
//        
//    return (float)num/len;
//} 


//static float offset_b =0.000;

////去除一个最大最小值取平均值滤波算法
////形参描述:
////ch:表示选择ADC的通道号
////len:表示取多少个值的平均值
//float GD30AD3344_ValueFiltrate(GD30AD3344_Channel_TypeDef ch,unsigned char len)
//{
//  uint8_t i;
//  float Min = 0,Max = 0;

//  float temp_data ;
//  float sampling[len];
//  float num = 0.0;
//  
//  for (i = 0; i < len; i++) 
//  {
//    
//    temp_data=GD30AD3344_AD_Read(ch,GD30AD3344_PGA_2V048);;
//    sampling[i] = temp_data;
//  }
//  
//  for (i = 0; i < len; i++) 
//  {
//    if(sampling[i] >= Max)
//      Max = sampling[i];
//    if(sampling[i] <= sampling[0])
//      Min = sampling[i];
//    
//    num += sampling[i];
//  }
////  printf("max=%f,min=%f\r\n",Max,Min);
//  return (float)(((num-Max-Min)/(len-2))-offset_b);                              //减去一个浮空的误差
//} 

