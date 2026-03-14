#include "sr501.h"

/*
 * 模块说明：SR501 数字输入读取实现。
 * 说明：本模块逻辑很简单，只负责 GPIO 初始化和电平读取，不维护任何滤波或去抖状态。
 */

/*
 * @brief  初始化 SR501 引脚
 * @note   配置为下拉输入：高电平表示检测到人体活动，低电平表示当前无人。
 */
void SR501_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(SR501_GPIO_CLK, ENABLE);
	GPIO_InitStructure.GPIO_Pin = SR501_GPIO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(SR501_GPIO_PORT, &GPIO_InitStructure);
}

/*
 * @brief  读取 SR501 检测结果
 * @retval 0 无人，1 有人
 */
uint16_t SR501_GetData(void)
{
	return GPIO_ReadInputDataBit(SR501_GPIO_PORT, SR501_GPIO_PIN);
}
