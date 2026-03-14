#ifndef __MQ2_H
#define __MQ2_H

/*
 * 模块说明：MQ-2 烟雾传感器模拟量采样驱动。
 * 运行方式：通过 ADC 读取 AO 输出电压，再根据经验公式估算烟雾浓度趋势。
 * 编码说明：本文件新增注释统一使用 UTF-8。
 */

#include "stm32f10x.h"
#include "delay.h"
#include "math.h"

/* MQ-2 模拟输出 AO 接口定义 */
#define MQ2_AO_GPIO_CLK		RCC_APB2Periph_GPIOB
#define MQ2_AO_GPIO_PORT	GPIOB
#define MQ2_AO_GPIO_PIN		GPIO_Pin_1			// MQ-2 AO -> PB1

/* ADC 采样通道定义 */
#define MQ2_AO_ADC			ADC1
#define MQ2_AO_ADC_CHANNEL	ADC_Channel_9	// ADC1_CH9 (PB1)

/* 每次读取时的平均采样次数 */
#define MQ2_READ_TIMES		10

/*
 * @brief  初始化 MQ-2 模拟采样通道（GPIO + ADC）
 */
void MQ2_Init(void);

/*
 * @brief  获取 MQ-2 原始 ADC 均值
 * @retval 0~4095（12 位 ADC）
 */
u16 MQ2_GetData(void);

/*
 * @brief  估算烟雾浓度（PPM）
 * @note   使用经验公式，结果用于趋势参考，不等同于标定仪器值
 */
float MQ2_GetData_PPM(void);

#endif
