/*
 * 模块说明：ESP8266 驱动接口定义（WiFi、AT 命令、收发缓冲）。
 * 使用边界：本模块只负责串口 AT 交互和数据收发，不处理 MQTT 业务语义。
 * 编码说明：本文件注释统一为 UTF-8。
 */

#ifndef _ESP8266_H_
#define _ESP8266_H_


#ifndef WIFI_SSID
#define WIFI_SSID		""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD	""
#endif

#define REV_OK		0
#define REV_WAIT	1


/* 功能：初始化 ESP8266，完成模块握手、工作模式配置、DHCP 打开和 WiFi 入网。 */
void ESP8266_Init(void);

/* 功能：清空 ESP8266 接收缓冲区。 */
void ESP8266_Clear(void);

/* 功能：发送 AT 指令并等待指定应答字符串，带固定超时轮询。 */
_Bool ESP8266_SendCmd(char *cmd, char *res);

/* 功能：通过 AT+CIPSEND 发送指定长度数据。 */
void ESP8266_SendData(unsigned char *data, unsigned short len);

/* 功能：从接收缓存中提取 +IPD 数据段的 payload 起始地址。 */
unsigned char *ESP8266_GetIPD(unsigned short timeOut);


#endif
