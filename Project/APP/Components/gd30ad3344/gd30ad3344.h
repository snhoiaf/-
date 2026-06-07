/*!
    \file    gd30ad3344.h
    \brief   definitions for the gd30ad3344
    
    \version 2024-6-27, V1.0.0, firmware for GD30AD3344
*/

#ifndef __GD30AD3344__H
#define __GD30AD3344__H

#ifdef __cplusplus
extern "C" {
#endif

#include "mcu_cmic_gd32f470vet6.h"

#define SPI_GD30AD3344          SPI3

#define SPI_GD30AD3344_CS_LOW()  gpio_bit_reset(SPI3_PORT, SPI3_NSS)
#define SPI_GD30AD3344_CS_HIGH() gpio_bit_set  (SPI3_PORT, SPI3_NSS)


typedef struct _GD30AD3344
{
    uint16_t SS         : 1;   //写状态:0无作用           1开始单次转换（默认）   
                          //读的时候总是返回0       
    uint16_t MUX        : 3;   // 0(默认)      1         2         3         4         5         6         7
                          //AIN0~AIN1 AIN0~AIN3 AIN1~AIN3 AIN2~AIN3 AIN0~GND  AIN1~GND  AIN2~GND  AIN3~GND  
    uint16_t PGA        : 3;   //    0         1       2(默认)     3         4         5         6         7
                          // ±6.144V   ±4.096V   ±2.048V   ±1.024V   ±0.512V   ±0.256V   ±0.256V  ±0.256V
    uint16_t MODE       : 1;   //0:连续转换模式    1:掉电，单次转换模式（默认）
    uint16_t DR         : 3;   //    0         1         2         3         4         5         6         7
                          //     6.25SPS     12.5SPS   25SPS     50SPS     100SPS    250SPS    500SPS    1000SPS
    uint16_t RESERVED_1 : 1;   //保留:写的时候写1，读的时候返回0或1 
    uint16_t PULL_UP_EN : 1;   //0:关闭DOUT引脚上拉电阻(默认)    1:开启DOUT引脚上拉电阻
    uint16_t NOP        : 2;   //0:不更新配置寄存器的数据  1:更新配置寄存器的数据(默认)  2:无效数据，且不更新配置寄存器数据
    uint16_t RESERVED   : 1;   //保留:写的时候写1，读的时候返回0或1 
}GD30AD3344;

extern GD30AD3344 GD30AD3344_InitStruct;
 

typedef enum
{
    GD30AD3344_PGA_6V144      = 0,
    GD30AD3344_PGA_4V096      = 1,
    GD30AD3344_PGA_2V048      = 2,
    GD30AD3344_PGA_1V024      = 3,
    GD30AD3344_PGA_0V512      = 4,
    GD30AD3344_PGA_0V256      = 5,
    GD30AD3344_PGA_0V064      = 6,
}

GD30AD3344_PGA_TypeDef;


typedef enum
{
    GD30AD3344_Channel_0      = 0,		//AIN0~AIN1
    GD30AD3344_Channel_1      = 1,		//AIN0~AIN3
    GD30AD3344_Channel_2      = 2,		//AIN1~AIN3
    GD30AD3344_Channel_3      = 3,		//AIN2~AIN3
    GD30AD3344_Channel_4      = 4,		//AIN0~GND
    GD30AD3344_Channel_5      = 5,		//AIN1~GND
    GD30AD3344_Channel_6      = 6,		//AIN2~GND
    GD30AD3344_Channel_7      = 7,		//AIN3~GND
}

GD30AD3344_Channel_TypeDef;

#define GD30AD3344_InitStruct_Value ((uint16_t)((GD30AD3344_InitStruct.SS         <<15)|\
                                                (GD30AD3344_InitStruct.MUX        <<12)|\
                                                (GD30AD3344_InitStruct.PGA        << 9)|\
                                                (GD30AD3344_InitStruct.MODE       << 8)|\
                                                (GD30AD3344_InitStruct.DR         << 5)|\
                                                (GD30AD3344_InitStruct.RESERVED_1 << 4)|\
                                                (GD30AD3344_InitStruct.PULL_UP_EN << 3)|\
                                                (GD30AD3344_InitStruct.NOP        << 1)|\
                                                (GD30AD3344_InitStruct.RESERVED   << 0)))

extern GD30AD3344 GD30AD3344_InitStruct;


void GD30AD3344_Init(void);

float GD30AD3344_AD_Read(GD30AD3344_Channel_TypeDef CH,GD30AD3344_PGA_TypeDef Ref);

float GD30AD3344_correct(GD30AD3344_Channel_TypeDef ch,unsigned char len);

#ifdef __cplusplus
}
#endif //__cplusplus
#endif //__GD30AD3344__H
