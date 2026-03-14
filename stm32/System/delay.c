#include "delay.h"
#include "misc.h"

/*
 * 模块说明：基于 SysTick 的阻塞延时实现。
 * 使用场景：DHT11 时序、ESP8266 轮询等待、OLED 上电稳定等待等需要短周期阻塞的地方。
 */

/*
 * 延时系数：
 * fac_us：1us 所需的 SysTick 计数值
 * fac_ms：1ms 所需的 SysTick 计数值
 */
static u8  fac_us = 0;
static u16 fac_ms = 0;

/*
 * @brief  初始化延时参数
 * @note   SysTick 时钟源选择 HCLK/8。
 *         fac_us 和 fac_ms 初始化后，后续 delay_us 和 delay_ms 都直接复用这两个系数。
 * @param  SYSCLK 系统时钟（MHz）
 */
void delay_init(u8 SYSCLK)
{
	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);
	fac_us = SYSCLK / 8;
	fac_ms = (u16)fac_us * 1000;
}								    

/*
 * @brief  毫秒级阻塞延时
 * @note   SysTick->LOAD 为 24 位，72MHz 下单次最大约 1864ms。
 *         本函数通过轮询 COUNTFLAG 判断计数是否结束，期间 CPU 会一直阻塞。
 * @param  nms 延时时长（ms）
 */
void delay_ms(u16 nms)
{	 		  	  
	u32 temp;		   
	SysTick->LOAD = (u32)nms * fac_ms;
	SysTick->VAL  = 0x00;
	SysTick->CTRL = 0x01;
	do
	{
		temp = SysTick->CTRL;
	}
	while (temp & 0x01 && !(temp & (1 << 16)));
	SysTick->CTRL = 0x00;
	SysTick->VAL  = 0x00;
}   

/*
 * @brief  微秒级阻塞延时
 * @param  nus 延时时长（us）
 * @note   适合对时序要求较高的驱动场景，例如 DHT11 读位和单总线握手。
 */
void delay_us(u32 nus)
{		
	u32 temp;	    	 
	SysTick->LOAD = nus * fac_us;
	SysTick->VAL  = 0x00;
	SysTick->CTRL = 0x01;
	do
	{
		temp = SysTick->CTRL;
	}
	while (temp & 0x01 && !(temp & (1 << 16)));
	SysTick->CTRL = 0x00;
	SysTick->VAL  = 0x00;
}

