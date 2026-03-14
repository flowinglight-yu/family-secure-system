#ifndef __SR501_H
#define __SR501_H

/*
 * 模块说明：SR501 人体红外传感器数字量输入驱动。
 * 运行方式：读取 DO 引脚高低电平，高电平表示当前检测到人体活动。
 * 编码说明：本文件新增注释统一使用 UTF-8。
 */

#include "stm32f10x.h"

/* SR501 数字输出接口定义（DO -> PA0） */
#define SR501_GPIO_CLK	RCC_APB2Periph_GPIOA
#define SR501_GPIO_PORT	GPIOA
#define SR501_GPIO_PIN	GPIO_Pin_0

/*
 * @brief  初始化 SR501 输入引脚
 */
void SR501_Init(void);

/*
 * @brief  读取人体检测状态
 * @retval 0 无人，1 有人
 */
uint16_t SR501_GetData(void);

#endif
