/*
 * 模块说明：ESP8266 驱动实现（AT 交互、发送、接收与中断缓存）。
 * 运行角色：负责把上层的字符串命令或原始数据转成串口发送，并把模块返回内容缓存在全局接收区。
 * 编码说明：本文件注释统一为 UTF-8。
 */

#include "stm32f10x.h"


#include "esp8266.h"


#include "delay.h"
#include "usart.h"


#include <string.h>
#include <stdio.h>


// 拼出连接目标 WiFi 的 AT 指令，初始化阶段直接用于入网。
#define ESP8266_WIFI_INFO		"AT+CWJAP=\"" WIFI_SSID "\",\"" WIFI_PASSWORD "\"\r\n"


// 中断接收缓冲区，ESP8266 返回的 AT 应答和 +IPD 数据都先进入这里。
unsigned char esp8266_buf[512];
// 当前累计接收字节数与上次轮询时的字节数，用于判断“接收是否已经稳定结束”。
unsigned short esp8266_cnt = 0, esp8266_cntPre = 0;


/* 功能：清空 ESP8266 接收缓存与计数器。 */
void ESP8266_Clear(void)
{

	memset(esp8266_buf, 0, sizeof(esp8266_buf));
	esp8266_cnt = 0;

}


/*
 * 功能：判断串口接收是否稳定结束。
 * 运行逻辑：
 *   1. 若当前还没有收到任何字节，则继续等待。
 *   2. 若本次计数与上次轮询计数相同，说明短时间内没有新字节到达，可视为一帧接收完成。
 *   3. 接收完成后清零计数，方便下一帧重新开始。
 */
_Bool ESP8266_WaitRecive(void)
{

	if(esp8266_cnt == 0)
		return REV_WAIT;

	if(esp8266_cnt == esp8266_cntPre)
	{
		esp8266_cnt = 0;

		return REV_OK;
	}

	esp8266_cntPre = esp8266_cnt;

	return REV_WAIT;

}


/*
 * 功能：发送 AT 指令并等待目标应答，超时返回失败。
 * 运行流程：
 *   1. 先把整条 AT 指令通过 USART2 发给 ESP8266。
 *   2. 以 10ms 为周期轮询接收缓冲区，最长等待约 2s。
 *   3. 一旦判断接收完成，就在缓冲区中查找目标应答字符串 res。
 *   4. 找到目标应答后清空接收缓存并返回成功，否则超时返回失败。
 */
_Bool ESP8266_SendCmd(char *cmd, char *res)
{

	unsigned char timeOut = 200;

	// 先完整发送 AT 命令，再进入轮询等待阶段。
	Usart_SendString(USART2, (unsigned char *)cmd, strlen((const char *)cmd));

	while(timeOut--)
	{
		if(ESP8266_WaitRecive() == REV_OK)
		{
			// 只有在接收稳定后才去查找目标应答，避免半包误判。
			if(strstr((const char *)esp8266_buf, res) != NULL)
			{
				ESP8266_Clear();

				return 0;
			}
		}

		delay_ms(10);
	}

	return 1;

}


/*
 * 功能：向 ESP8266 申请发送窗口后下发原始数据。
 * 说明：该函数会先发送 AT+CIPSEND=len，等到模块返回 '>' 提示符后再真正发送数据体。
 */
void ESP8266_SendData(unsigned char *data, unsigned short len)
{

	char cmdBuf[32];

	ESP8266_Clear();
	sprintf(cmdBuf, "AT+CIPSEND=%d\r\n", len);
	if(!ESP8266_SendCmd(cmdBuf, ">"))
	{
		Usart_SendString(USART2, data, len);
	}
	else
	{
		UsartPrintf(USART_DEBUG, "WARN:\tCIPSEND no prompt\r\n");
	}

}


/*
 * 功能：解析并返回 +IPD 数据段的 payload 起始地址。
 * 运行流程：
 *   1. 轮询等待接收完成。
 *   2. 在接收缓存中查找 'IPD,' 片段，确认这是透传回来的网络数据。
 *   3. 继续查找 ':'，其后面的内容就是 MQTT 或 HTTP 负载起始位置。
 */
unsigned char *ESP8266_GetIPD(unsigned short timeOut)
{

	char *ptrIPD = NULL;

	do
	{
		if(ESP8266_WaitRecive() == REV_OK)
		{
			ptrIPD = strstr((char *)esp8266_buf, "IPD,");
			if(ptrIPD == NULL)
			{

			}
			else
			{
				ptrIPD = strchr(ptrIPD, ':');
				if(ptrIPD != NULL)
				{
					ptrIPD++;
					return (unsigned char *)(ptrIPD);
				}
				else
					return NULL;

			}
		}

		delay_ms(5);
	} while(timeOut--);

	return NULL;

}


/*
 * 功能：执行 ESP8266 上电初始化与入网流程。
 * 运行流程：
 *   1. 发送 AT，确认模块在线。
 *   2. 配置 STA 模式，使模块以客户端身份接入路由器。
 *   3. 打开 DHCP，确保模块自动获取 IP。
 *   4. 使用预设 SSID 和密码连接目标 WiFi，直到收到 GOT IP。
 */
void ESP8266_Init(void)
{

	ESP8266_Clear();

	// 第一步：确认 ESP8266 已经上电并能正确响应基础 AT 指令。
	while(ESP8266_SendCmd("AT\r\n", "OK"))
		delay_ms(500);

	// 第二步：切换到 Station 模式，准备接入外部路由器。
	UsartPrintf(USART_DEBUG, "ESP8266 step2: CWMODE\r\n");
	while(ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK"))
		delay_ms(500);

	// 第三步：开启 DHCP 自动获取 IP 地址。
	UsartPrintf(USART_DEBUG, "ESP8266 step3: CWDHCP\r\n");
	while(ESP8266_SendCmd("AT+CWDHCP=1,1\r\n", "OK"))
		delay_ms(500);

	// 第四步：连接目标热点，直到模块回包中出现 GOT IP。
	UsartPrintf(USART_DEBUG, "ESP8266 step4: CWJAP\r\n");
	while(ESP8266_SendCmd(ESP8266_WIFI_INFO, "GOT IP"))
		delay_ms(500);

	UsartPrintf(USART_DEBUG, "5. ESP8266 Init OK\r\n");
	delay_ms(500);

}


/*
 * 功能：USART2 接收中断，持续写入 ESP8266 接收缓存。
 * 说明：
 *   1. 每收到 1 个字节就追加到全局缓冲区末尾。
 *   2. 若缓冲区写满，则计数回绕到 0，避免越界访问。
 *   3. 上层轮询函数通过计数变化判断一帧数据是否已经接收完毕。
 */
void USART2_IRQHandler(void)
{

	if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
	{
		if(esp8266_cnt >= sizeof(esp8266_buf))	esp8266_cnt = 0;
		esp8266_buf[esp8266_cnt++] = USART2->DR;

		USART_ClearFlag(USART2, USART_FLAG_RXNE);
	}

}
