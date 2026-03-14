#include "dht11.h"
#include "delay.h"

/*
 * 模块说明：DHT11 单总线时序读取实现。
 * 关键点：
 *   1. 主机通过 GPIO 输出模式拉低总线，发送起始信号。
 *   2. 传感器响应后切换为输入模式，按高电平持续时间判定 0 或 1。
 *   3. 一帧共 5 字节，最后 1 字节用于前 4 字节累加和校验。
 */

/*
 * @brief  发送 DHT11 起始信号
 * @note   主机先拉低总线至少 18ms，再拉高并等待传感器响应。
 */
void DHT11_Rst(void)
{
	DHT11_Mode(OUT);
	DHT11_Low;
	delay_ms(20);
	DHT11_High;
	delay_us(13);
}

/*
 * @brief  等待 DHT11 响应
 * @note   典型响应：先低电平 40~80us，再高电平 40~80us
 * @retval 0 响应正常，1 超时失败
 */
u8 DHT11_Check(void)
{
	u8 retry = 0;
	DHT11_Mode(IN);

	while (GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN) && retry < 100)
	{
		retry++;
		delay_us(1);
	}
	if (retry >= 100)
	{
		return 1;
	}

	retry = 0;
	while (!GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN) && retry < 100)
	{
		retry++;
		delay_us(1);
	}
	if (retry >= 100)
	{
		return 1;
	}

	return 0;
}

/*
 * @brief  读取 DHT11 的 1 位数据
 * @note   运行流程：
 *         1. 等待当前位起始前的高电平结束。
 *         2. 等待 50us 左右的低电平引导脉冲结束。
 *         3. 在后续高电平阶段延时约 40us 采样，高电平仍存在则判为 1，否则判为 0。
 * @retval 读取到的位值（0 或 1）
 */
u8 DHT11_Read_Bit(void)
{
	u8 retry = 0;

	while (GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN) && retry < 100)
	{
		retry++;
		delay_us(1);
	}

	retry = 0;
	while (!GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN) && retry < 100)
	{
		retry++;
		delay_us(1);
	}

	delay_us(40);
	if (GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN))
	{
		return 1;
	}
	return 0;
}

/*
 * @brief  读取 DHT11 的 1 字节（8 位）
 * @note   DHT11 采用高位在前的传输顺序，因此每次先左移再并入新读出的位值。
 * @retval 读取到的字节数据
 */
u8 DHT11_Read_Byte(void)
{
	u8 i;
	u8 dat = 0;

	for (i = 0; i < 8; i++)
	{
		dat <<= 1;
		dat |= DHT11_Read_Bit();
	}

	return dat;
}

/*
 * @brief  读取 DHT11 温湿度
 * @param  temp 温度输出（整数部分）
 * @param  humi 湿度输出（整数部分）
 * @retval 0 成功，1 失败
 * @note   运行流程：
 *         1. 先发送起始信号并等待 DHT11 响应。
 *         2. 连续读取 5 个字节，顺序为湿度整数、湿度小数、温度整数、温度小数、校验。
 *         3. 对前 4 字节求和并与校验字节比较，校验通过后才更新输出参数。
 */
u8 DHT11_Read_Data(u8 *temp, u8 *humi)
{
	u8 buf[5];
	u8 i;

	DHT11_Rst();
	if (DHT11_Check() == 0)
	{
		for (i = 0; i < 5; i++)
		{
			buf[i] = DHT11_Read_Byte();
		}

		/* 校验：前 4 字节累加和应等于第 5 字节。 */
		if ((buf[0] + buf[1] + buf[2] + buf[3]) == buf[4])
		{
			*humi = buf[0];
			*temp = buf[2];
		}
	}
	else
	{
		return 1;
	}

	return 0;
}

/*
 * @brief  初始化 DHT11 GPIO 并进行一次握手检测
 * @retval 0 成功，1 失败
 * @note   初始化时先把数据线配置成输出高电平空闲态，再主动发送一次起始时序确认传感器在线。
 */
u8 DHT11_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(DHT11_GPIO_CLK, ENABLE);
	GPIO_InitStructure.GPIO_Pin = DHT11_GPIO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(DHT11_GPIO_PORT, &GPIO_InitStructure);
	GPIO_SetBits(DHT11_GPIO_PORT, DHT11_GPIO_PIN);

	DHT11_Rst();
	return DHT11_Check();
}

/*
 * @brief  切换 DHT11 数据线模式
 * @param  mode OUT：推挽输出；IN：浮空输入
 * @note   起始时序阶段需要主机驱动总线，读数据阶段必须切回输入模式让传感器占用总线。
 */
void DHT11_Mode(u8 mode)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Pin = DHT11_GPIO_PIN;
	if (mode)
	{
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	}
	else
	{
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	}

	GPIO_Init(DHT11_GPIO_PORT, &GPIO_InitStructure);
}

