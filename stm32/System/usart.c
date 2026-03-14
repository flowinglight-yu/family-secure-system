/**
	************************************************************
	* 文件名称: usart.c
	*
	* 模块作用: 串口初始化、阻塞发送和格式化调试输出。
	*
	* 使用说明:
	*   1. USART1 默认作为调试串口，连接 PA9/PA10。
	*   2. USART2 默认作为 ESP8266 通信串口，连接 PA2/PA3。
	*   3. 本文件只负责底层串口收发和初始化，不维护复杂协议状态。
	*
	* 编码说明: 本文件新增注释统一使用 UTF-8。
	************************************************************
**/

// 硬件驱动头文件
#include "usart.h"
#include "delay.h"

// C 标准库头文件
#include <stdarg.h>
#include <string.h>
#include <stdio.h>


/*
************************************************************
*	函数名称:	Usart1_Init
*
*	函数作用:	初始化 USART1，用于调试信息输出和接收。
*
*	输入参数:	baud - 串口波特率，例如 115200。
*
*	返 回 值:	无
*
*	说明:		TX 使用 PA9，RX 使用 PA10。
*			函数会完成时钟打开、GPIO 模式配置、串口参数配置
*			以及接收中断使能。
************************************************************
*/
void Usart1_Init(unsigned int baud)
{

	GPIO_InitTypeDef gpio_initstruct;
	USART_InitTypeDef usart_initstruct;
	NVIC_InitTypeDef nvic_initstruct;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	
	// 配置 PA9 为复用推挽输出，对应 USART1_TX。
	gpio_initstruct.GPIO_Mode = GPIO_Mode_AF_PP;
	gpio_initstruct.GPIO_Pin = GPIO_Pin_9;
	gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpio_initstruct);
	
	// 配置 PA10 为浮空输入，对应 USART1_RX。
	gpio_initstruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	gpio_initstruct.GPIO_Pin = GPIO_Pin_10;
	gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpio_initstruct);
	
	usart_initstruct.USART_BaudRate = baud;
	usart_initstruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;		// 不使用硬件流控。
	usart_initstruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;						// 同时开启接收和发送。
	usart_initstruct.USART_Parity = USART_Parity_No;									// 无奇偶校验。
	usart_initstruct.USART_StopBits = USART_StopBits_1;								// 使用 1 位停止位。
	usart_initstruct.USART_WordLength = USART_WordLength_8b;							// 使用 8 位数据位。
	USART_Init(USART1, &usart_initstruct);
	
	USART_Cmd(USART1, ENABLE);													// 使能 USART1 外设。
	
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);									// 开启接收非空中断。
	
	nvic_initstruct.NVIC_IRQChannel = USART1_IRQn;
	nvic_initstruct.NVIC_IRQChannelCmd = ENABLE;
	nvic_initstruct.NVIC_IRQChannelPreemptionPriority = 0;
	nvic_initstruct.NVIC_IRQChannelSubPriority = 2;
	NVIC_Init(&nvic_initstruct);

}

/*
************************************************************
*	函数名称:	Usart2_Init
*
*	函数作用:	初始化 USART2，用于 STM32 与 ESP8266 的串口通信。
*
*	输入参数:	baud - 串口波特率。
*
*	返 回 值:	无
*
*	说明:		TX 使用 PA2，RX 使用 PA3。
*			该串口承担网络模块的 AT 指令收发，因此中断优先级
*			高于调试串口子优先级。
************************************************************
*/
void Usart2_Init(unsigned int baud)
{

	GPIO_InitTypeDef gpio_initstruct;
	USART_InitTypeDef usart_initstruct;
	NVIC_InitTypeDef nvic_initstruct;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
	
	// 配置 PA2 为复用推挽输出，对应 USART2_TX。
	gpio_initstruct.GPIO_Mode = GPIO_Mode_AF_PP;
	gpio_initstruct.GPIO_Pin = GPIO_Pin_2;
	gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpio_initstruct);
	
	// 配置 PA3 为浮空输入，对应 USART2_RX。
	gpio_initstruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	gpio_initstruct.GPIO_Pin = GPIO_Pin_3;
	gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpio_initstruct);
	
	usart_initstruct.USART_BaudRate = baud;
	usart_initstruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;		// 不使用硬件流控。
	usart_initstruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;						// 同时开启接收和发送。
	usart_initstruct.USART_Parity = USART_Parity_No;									// 无奇偶校验。
	usart_initstruct.USART_StopBits = USART_StopBits_1;								// 使用 1 位停止位。
	usart_initstruct.USART_WordLength = USART_WordLength_8b;							// 使用 8 位数据位。
	USART_Init(USART2, &usart_initstruct);
	
	USART_Cmd(USART2, ENABLE);													// 使能 USART2 外设。
	
	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);									// 开启接收非空中断。
	
	nvic_initstruct.NVIC_IRQChannel = USART2_IRQn;
	nvic_initstruct.NVIC_IRQChannelCmd = ENABLE;
	nvic_initstruct.NVIC_IRQChannelPreemptionPriority = 0;
	nvic_initstruct.NVIC_IRQChannelSubPriority = 0;
	NVIC_Init(&nvic_initstruct);

}

/*
************************************************************
*	函数名称:	Usart_SendString
*
*	函数作用:	按给定长度逐字节发送一段缓冲区数据。
*
*	输入参数:	USARTx - 目标串口。
*				str    - 待发送的数据首地址。
*				len    - 发送字节数。
*
*	返 回 值:	无
*
*	说明:		本函数采用阻塞方式发送，每发出 1 个字节都会等待
*			发送完成标志位置位，因此适合调试打印和短报文发送。
************************************************************
*/
void Usart_SendString(USART_TypeDef *USARTx, unsigned char *str, unsigned short len)
{

	unsigned short count = 0;
	
	for(; count < len; count++)
	{
		USART_SendData(USARTx, *str++);									// 发送当前字节。
		while(USART_GetFlagStatus(USARTx, USART_FLAG_TC) == RESET);		// 等待当前字节发送完成。
	}

}

/*
************************************************************
*	函数名称:	UsartPrintf
*
*	函数作用:	先将可变参数格式化到本地缓存，再通过串口逐字节发出。
*
*	输入参数:	USARTx - 目标串口。
*				fmt    - printf 风格格式字符串。
*
*	返 回 值:	无
*
*	说明:		该接口主要用于调试输出，缓存长度固定为 296 字节。
*			若格式化后的内容超出缓存，vsnprintf 会自动截断。
************************************************************
*/
void UsartPrintf(USART_TypeDef *USARTx, char *fmt,...)
{

	unsigned char UsartPrintfBuf[296];
	va_list ap;
	unsigned char *pStr = UsartPrintfBuf;
	
	va_start(ap, fmt);
	vsnprintf((char *)UsartPrintfBuf, sizeof(UsartPrintfBuf), fmt, ap);							// 将格式化结果写入本地缓存。
	va_end(ap);
	
	while(*pStr != 0)
	{
		USART_SendData(USARTx, *pStr++);
		while(USART_GetFlagStatus(USARTx, USART_FLAG_TC) == RESET);
	}

}

/*
************************************************************
*	函数名称:	USART1_IRQHandler
*
*	函数作用:	USART1 接收中断服务函数。
*
*	输入参数:	无
*
*	返 回 值:	无
*
*	说明:		当前工程中 USART1 主要用于调试打印，因此这里只清除
*			接收标志，避免中断重复进入；若后续需要接收调试命令，
*			可在此扩展接收缓冲逻辑。
************************************************************
*/
void USART1_IRQHandler(void)
{

	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) // 接收到新字节后进入中断。
	{
		USART_ClearFlag(USART1, USART_FLAG_RXNE);
	}

}
