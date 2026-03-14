#ifndef __DELAY_H
#define __DELAY_H 			   
#include "stm32f10x.h"

/*
 * 延时模块（基于 SysTick）
 * - delay_init：根据系统主频计算 us/ms 延时系数
 * - delay_us  ：微秒级阻塞延时
 * - delay_ms  ：毫秒级阻塞延时
 * 说明：本模块采用忙等待方式，占用 CPU，适合短延时和驱动时序控制。
 */

/*
 * @brief  初始化延时模块
 * @param  SYSCLK 系统主频，单位 MHz（例如 72）
 */
void delay_init(u8 SYSCLK);

/*
 * @brief  毫秒级阻塞延时
 * @param  nms 延时时长，单位 ms
 */
void delay_ms(u16 nms);

/*
 * @brief  微秒级阻塞延时
 * @param  nus 延时时长，单位 us
 */
void delay_us(u32 nus);

#endif
