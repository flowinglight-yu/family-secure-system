#ifndef _USART_H_
#define _USART_H_


/*
 * 模块说明: 串口驱动接口声明。
 * 说明: USART1 用于调试打印，USART2 用于 ESP8266 通信。
 * 编码说明: 本文件新增注释统一使用 UTF-8。
 */

#include "stm32f10x.h"


#define USART_DEBUG		USART1		// 调试打印默认使用 USART1。


/* 功能: 初始化调试串口 USART1。 */
void Usart1_Init(unsigned int baud);

/* 功能: 初始化与 ESP8266 通信的 USART2。 */
void Usart2_Init(unsigned int baud);

/* 功能: 按指定长度阻塞发送一段缓冲区数据。 */
void Usart_SendString(USART_TypeDef *USARTx, unsigned char *str, unsigned short len);

/* 功能: 按 printf 风格格式化并输出调试信息。 */
void UsartPrintf(USART_TypeDef *USARTx, char *fmt,...);

#endif
