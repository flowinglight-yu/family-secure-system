#include "mq2.h"

/*
 * 模块说明：MQ-2 ADC 采样与 PPM 估算实现。
 * 关键点：
 *   1. 原始 ADC 值用于本地显示和阈值判断。
 *   2. PPM 值使用经验公式换算，更适合做趋势参考而不是精确标定值。
 */

/*
 * @brief  执行一次 ADC 规则组转换并返回结果
 * @retval 当前采样值（12 位）
 * @note   每次调用都会重新指定规则组通道并启动一次软件触发转换。
 */
static u16 MQ2_ADC_Read(void)
{
	ADC_RegularChannelConfig(MQ2_AO_ADC, MQ2_AO_ADC_CHANNEL, 1, ADC_SampleTime_55Cycles5);
	ADC_SoftwareStartConvCmd(MQ2_AO_ADC, ENABLE);
	while (ADC_GetFlagStatus(MQ2_AO_ADC, ADC_FLAG_EOC) == RESET);
	return ADC_GetConversionValue(MQ2_AO_ADC);
}

/*
 * @brief  初始化 MQ-2 对应 GPIO 与 ADC 外设
 * @note   运行流程：
 *         1. 打开 GPIOB 和 ADC1 时钟，并把 ADC 时钟分频到合适范围。
 *         2. 将 PB1 配置为模拟输入。
 *         3. 初始化 ADC1 为独立、单次、单通道采样模式。
 *         4. 执行复位校准和启动校准，保证后续采样结果稳定。
 */
void MQ2_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	ADC_InitTypeDef ADC_InitStructure;

	RCC_APB2PeriphClockCmd(MQ2_AO_GPIO_CLK | RCC_APB2Periph_ADC1, ENABLE);
	RCC_ADCCLKConfig(RCC_PCLK2_Div6);

	GPIO_InitStructure.GPIO_Pin = MQ2_AO_GPIO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(MQ2_AO_GPIO_PORT, &GPIO_InitStructure);

	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;
	ADC_InitStructure.ADC_NbrOfChannel = 1;
	ADC_Init(MQ2_AO_ADC, &ADC_InitStructure);

	ADC_Cmd(MQ2_AO_ADC, ENABLE);
	ADC_ResetCalibration(MQ2_AO_ADC);
	while (ADC_GetResetCalibrationStatus(MQ2_AO_ADC) == SET);
	ADC_StartCalibration(MQ2_AO_ADC);
	while (ADC_GetCalibrationStatus(MQ2_AO_ADC) == SET);
}

/*
 * @brief  获取 MQ-2 原始 ADC 平均值
 * @note   通过多次采样求均值降低抖动，适合直接作为原始烟雾强度参考值使用。
 */
u16 MQ2_GetData(void)
{
	u32 tempData = 0;
	u8 i;

	for (i = 0; i < MQ2_READ_TIMES; i++)
	{
		tempData += MQ2_ADC_Read();
		delay_ms(5);
	}

	return (u16)(tempData / MQ2_READ_TIMES);
}

/*
 * @brief  计算 MQ-2 估算浓度（PPM）
 * @note   运行流程：
 *         1. 先对 ADC 做多次采样求平均，降低瞬时噪声。
 *         2. 将 ADC 结果换算成 AO 输出电压 vol。
 *         3. 根据分压关系计算传感器当前电阻 rs。
 *         4. 使用经验公式推算 PPM，结果主要用于趋势分析。
 *
 *         公式说明：
 *         - vol = ADC * 5.0 / 4096
 *         - rs = (5.0 - vol) / (vol * 0.5)
 *         - ppm = pow(11.5428 * r0 / rs, 0.6549)
 */
float MQ2_GetData_PPM(void)
{
	float tempData = 0.0f;
	float vol;
	float rs;
	float r0 = 6.64f;
	u8 i;

	for (i = 0; i < MQ2_READ_TIMES; i++)
	{
		tempData += MQ2_ADC_Read();
		delay_ms(5);
	}
	tempData /= MQ2_READ_TIMES;

	vol = (tempData * 5.0f / 4096.0f);
	rs = (5.0f - vol) / (vol * 0.5f);

	return (float)pow(11.5428f * r0 / rs, 0.6549f);
}
