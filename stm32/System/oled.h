#ifndef __OLED_H
#define __OLED_H

/*
 * 模块说明：OLED 显示驱动接口，使用 GPIO 模拟 I2C 与 SSD1306 兼容控制器通信。
 * 坐标体系：本项目把 128x64 像素划分为 4 行字符区，每个 ASCII 字符占 8x16，每个汉字占 16x16。
 * 编码说明：本文件新增注释统一使用 UTF-8。
 */

#include "stm32f10x.h"

/*
 * OLED 引脚定义（软件 I2C）
 * - SCL: PB8
 * - SDA: PB9
 */
#define OLED_SCL			GPIO_Pin_8
#define OLED_SDA			GPIO_Pin_9
#define OLED_PROT  			GPIOB

/* 软件 I2C 线电平写入宏 */
#define OLED_W_SCL(x)		GPIO_WriteBit(OLED_PROT, OLED_SCL, (BitAction)(x))
#define OLED_W_SDA(x)		GPIO_WriteBit(OLED_PROT, OLED_SDA, (BitAction)(x))

/*
 * @brief  初始化 OLED 控制器和显示参数
 */
void OLED_Init(void);

/* @brief  清空整屏内容 */
void OLED_Clear(void);

/*
 * @brief  显示单个 ASCII 字符（8x16）
 * @param  Line 行号（1~4）
 * @param  Column 列号（1~16）
 * @param  Char 字符
 */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);

/*
 * @brief  显示字符串
 * @param  Line 起始行（1~4）
 * @param  Column 起始列（1~16）
 */
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);

/* @brief  显示无符号十进制数 */
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);

/* @brief  显示有符号十进制数 */
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);

/* @brief  显示十六进制数 */
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);

/* @brief  显示二进制数 */
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);

/*
 * @brief  显示 16x16 汉字
 * @param  Line 行号（1~4）
 * @param  Column 列号（1~8）
 * @param  num 字模索引（参见 OLED_Font.h 中 Hzk1）
 */
void OLED_ShowChinese(uint8_t Line, uint8_t Column, uint8_t num);
#endif
