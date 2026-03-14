/*
 * 模块说明：主程序入口，负责硬件初始化、显示刷新、联网建链和周期上报。
 * 运行节拍：主循环每 200ms 执行 1 次，累计 25 次后触发一次属性上报。
 * 编码说明：本文件新增注释统一使用 UTF-8。
 */

#include "stm32f10x.h"
#include "delay.h"
#include "dht11.h"
#include "oled.h"
#include "mq2.h"
#include "sr501.h"
#include "usart.h"
#include "esp8266.h"
#include "onenet.h"

// 建立到 OneNET MQTT 接入点的 TCP 连接，后续 MQTT 报文都通过该连接发送。
#define ESP8266_ONENET_INFO	"AT+CIPSTART=\"TCP\",\"mqtts.heclouds.com\",1883\r\n"

// 全局传感器缓存，供本地显示和 OneNET 上报共用。
u8 temp = 0;
u8 humi = 0;
u16 mq2Value = 0;
float mq2PpmValue = 0.0f;
u16 bodyValue = 0;

// 记录上一次人体检测结果，避免 OLED 重复刷新同一内容。
static u16 bodyPreValue = 2;

/*
 * 功能：完成 MCU 基础硬件初始化。
 * 运行流程：
 *   1. 配置中断优先级分组和 SysTick 延时基准。
 *   2. 初始化调试串口和 ESP8266 串口。
 *   3. 初始化 OLED、MQ2、SR501。
 *   4. 循环握手 DHT11，直到传感器响应正常。
 */
static void Hardware_Init(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	delay_init(72);

	Usart1_Init(115200);
	Usart2_Init(115200);

	OLED_Init();
	MQ2_Init();
	SR501_Init();

	while (DHT11_Init())
	{
		delay_ms(1000);
	}
}

/*
 * 功能：初始化 OLED 首页固定标签。
 * 说明：这里只绘制不会频繁变化的标题和单位，实时数据由 Refresh_SensorAndDisplay 更新。
 */
static void Display_Init(void)
{
	OLED_Clear();

	// 第 1 行显示温度标签和单位。
	OLED_ShowChinese(1, 1, 0);
	OLED_ShowChinese(1, 2, 1);
	OLED_ShowChar(1, 5, ':');
	OLED_ShowChar(1, 9, 'C');

	// 第 2 行显示湿度标签和单位。
	OLED_ShowChinese(2, 1, 2);
	OLED_ShowChinese(2, 2, 1);
	OLED_ShowChar(2, 5, ':');
	OLED_ShowChar(2, 9, '%');

	// 第 3 行显示烟雾原始值标签。
	OLED_ShowString(3, 1, "SMOKE:");

	// 第 4 行显示人体检测状态标签。
	OLED_ShowChinese(4, 1, 13);
	OLED_ShowChinese(4, 2, 14);
	OLED_ShowChar(4, 5, ':');
}

/*
 * 功能：刷新传感器缓存并同步更新 OLED。
 * 运行流程：
 *   1. 读取 DHT11 温湿度，失败时显示 -- 并重新初始化 DHT11。
 *   2. 更新 MQ2 原始值和 PPM 估算值。
 *   3. 读取 SR501 状态，仅在状态变化时刷新“有人/无人”显示。
 */
static void Refresh_SensorAndDisplay(void)
{
	if (DHT11_Read_Data(&temp, &humi) == 0)
	{
		OLED_ShowNum(1, 6, temp, 2);
		OLED_ShowNum(2, 6, humi, 2);
	}
	else
	{
		OLED_ShowString(1, 6, "--");
		OLED_ShowString(2, 6, "--");
		DHT11_Init();
	}

	mq2Value = MQ2_GetData();
	mq2PpmValue = MQ2_GetData_PPM();
	OLED_ShowNum(3, 7, mq2Value, 4);

	bodyValue = SR501_GetData();
	if (bodyValue != bodyPreValue)
	{
		if (bodyValue)
		{
			OLED_ShowChinese(4, 6, 13);
			OLED_ShowChinese(4, 7, 14);
		}
		else
		{
			OLED_ShowChinese(4, 6, 15);
			OLED_ShowChinese(4, 7, 14);
		}
		bodyPreValue = bodyValue;
	}
}

/*
 * 功能：系统主入口。
 * 运行流程：
 *   1. 初始化硬件和 OLED 静态界面。
 *   2. 初始化 ESP8266，并建立到 OneNET MQTT 服务器的 TCP 连接。
 *   3. 完成 MQTT 鉴权建链并订阅属性下发主题。
 *   4. 主循环中每 200ms 刷新一次本地数据，并每 5s 上报一次属性。
 */
int main(void)
{
	unsigned short publishTick = 0;
	unsigned char *dataPtr = 0;

	// 第一步：完成本地硬件和显示初始化。
	Hardware_Init();
	Display_Init();
	UsartPrintf(USART_DEBUG, "WiFi SSID: %s\r\n", WIFI_SSID);
	UsartPrintf(USART_DEBUG, "OneNET Product: %s, Device: %s\r\n", ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);

	// 第二步：初始化 WiFi 模块并创建到底层 MQTT 服务器的 TCP 连接。
	ESP8266_Init();
	UsartPrintf(USART_DEBUG, "Connect MQTTs Server...\r\n");
	while (ESP8266_SendCmd(ESP8266_ONENET_INFO, "CONNECT"))
	{
		UsartPrintf(USART_DEBUG, "WARN: MQTT TCP connect retry\r\n");
		delay_ms(500);
	}
	UsartPrintf(USART_DEBUG, "Connect MQTT Server Success\r\n");

	// 第三步：完成 OneNET 鉴权建链，成功后订阅属性设置主题。
	UsartPrintf(USART_DEBUG, "Connect OneNET MQTT...\r\n");
	while (OneNet_DevLink())
	{
		UsartPrintf(USART_DEBUG, "WARN: OneNET MQTT connect retry\r\n");
		ESP8266_SendCmd(ESP8266_ONENET_INFO, "CONNECT");
		delay_ms(500);
	}
	UsartPrintf(USART_DEBUG, "OneNET MQTT Connect Success\r\n");

	OneNET_Subscribe();

	// 第四步：进入主循环，维持本地显示、周期上报和下行报文处理。
	while (1)
	{
		Refresh_SensorAndDisplay();

		// 25 * 200ms = 5s，按 5 秒节拍上报一次当前属性。
		if (++publishTick >= 25)
		{
			OneNet_SendData();
			publishTick = 0;
			ESP8266_Clear();
		}

		// 若 ESP8266 接收到 +IPD 数据，则交给 MQTT/OneNET 解析流程处理。
		dataPtr = ESP8266_GetIPD(0);
		if (dataPtr != 0)
			OneNet_RevPro(dataPtr);

		// 主循环固定节拍，兼顾显示刷新和联网轮询。
		delay_ms(200);
	}
}
