#ifndef __DHT11_H
#define __DHT11_H

/*
 * 模块说明：DHT11 单总线温湿度采集驱动。
 * 运行方式：主机先发送起始时序，再读取 40 位数据帧，最终得到湿度整数、小数、温度整数、小数和校验字节。
 * 编码说明：本文件新增注释统一使用 UTF-8。
 */

#include "stm32f10x.h"
#include "delay.h"

/* DHT11 单总线引脚定义 */
#define DHT11_GPIO_PORT  GPIOB
#define DHT11_GPIO_PIN   GPIO_Pin_10
#define DHT11_GPIO_CLK   RCC_APB2Periph_GPIOB

/* GPIO 方向配置 */
#define OUT 1
#define IN  0

/* 通过拉高/拉低数据线发送时序 */
#define DHT11_Low  GPIO_ResetBits(DHT11_GPIO_PORT, DHT11_GPIO_PIN)
#define DHT11_High GPIO_SetBits(DHT11_GPIO_PORT, DHT11_GPIO_PIN)

/*
 * @brief  初始化 DHT11（含握手检测）
 * @retval 0 成功，1 失败
 */
u8 DHT11_Init(void);

/*
 * @brief  读取温湿度数据
 * @param  temp 温度输出（单位：℃）
 * @param  humi 湿度输出（单位：%RH）
 * @retval 0 成功，1 失败
 */
u8 DHT11_Read_Data(u8 *temp, u8 *humi);

/* @brief  读取一个字节（8 位） */
u8 DHT11_Read_Byte(void);

/* @brief  读取一位数据 */
u8 DHT11_Read_Bit(void);

/*
 * @brief  配置 DHT11 数据引脚方向
 * @param  mode OUT（输出）或 IN（输入）
 */
void DHT11_Mode(u8 mode);

/*
 * @brief  检测 DHT11 响应信号
 * @retval 0 有效响应，1 响应超时
 */
u8 DHT11_Check(void);

/* @brief  发送起始信号（复位时序） */
void DHT11_Rst(void);

#endif
