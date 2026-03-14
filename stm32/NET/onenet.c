/*
 * 模块说明：OneNET 认证、连接、属性上报与下行处理实现。
 * 运行角色：在 ESP8266 串口透传之上完成鉴权参数生成、MQTT 建链、属性上报和平台应答解析。
 * 编码说明：本文件注释统一为 UTF-8。
 */

#include "stm32f10x.h"


#include "esp8266.h"


#include "onenet.h"
#include "MqttKit.h"


#include "base64.h"
#include "hmac_sha1.h"


#include "usart.h"
#include "delay.h"


#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define PROID			ONENET_PRODUCT_ID

#define ACCESS_KEY		ONENET_ACCESS_KEY

#define DEVICE_NAME		ONENET_DEVICE_NAME


char devid[16];

char key[48];


extern unsigned char esp8266_buf[512];

extern u8 temp;
extern u8 humi;
extern u16 mq2Value;
extern float mq2PpmValue;
extern u16 bodyValue;


/*
 * 功能：对签名字符串进行 URL 编码，满足 HTTP/OneNET 认证参数要求。
 * 运行流程：
 *   1. 先把原始签名复制到临时缓冲区，避免原地覆盖导致后续字符丢失。
 *   2. 再按字符逐个扫描，把 +、/、= 等保留字符替换成 %XX 形式。
 *   3. 最终结果仍写回 sign，供后续 Authorization 参数直接使用。
 */
static unsigned char OTA_UrlEncode(char *sign)
{

	char sign_t[40];
	unsigned char i = 0, j = 0;
	unsigned char sign_len = strlen(sign);

	if(sign == (void *)0 || sign_len < 28)
		return 1;

	for(; i < sign_len; i++)
	{
		sign_t[i] = sign[i];
		sign[i] = 0;
	}
	sign_t[i] = 0;

	for(i = 0, j = 0; i < sign_len; i++)
	{
		switch(sign_t[i])
		{
			case '+':
				strcat(sign + j, "%2B");j += 3;
			break;

			case ' ':
				strcat(sign + j, "%20");j += 3;
			break;

			case '/':
				strcat(sign + j, "%2F");j += 3;
			break;

			case '?':
				strcat(sign + j, "%3F");j += 3;
			break;

			case '%':
				strcat(sign + j, "%25");j += 3;
			break;

			case '#':
				strcat(sign + j, "%23");j += 3;
			break;

			case '&':
				strcat(sign + j, "%26");j += 3;
			break;

			case '=':
				strcat(sign + j, "%3D");j += 3;
			break;

			default:
				sign[j] = sign_t[i];j++;
			break;
		}
	}

	sign[j] = 0;

	return 0;

}


#define METHOD		"sha1"
/*
 * 功能：生成 OneNET 鉴权 Authorization 字符串。
 * 运行流程：
 *   1. 先把 Base64 编码保存的 access key 解码成原始密钥。
 *   2. 按 OneNET 要求拼出 string_for_signature，内容包含过期时间、签名方法、资源路径和版本号。
 *   3. 使用 HMAC-SHA1 计算签名结果，再进行一次 Base64 编码。
 *   4. 对 Base64 结果做 URL 编码，最后拼出完整 Authorization 查询串。
 */
static unsigned char OneNET_Authorization(char *ver, char *res, unsigned int et, char *access_key, char *dev_name,
											char *authorization_buf, unsigned short authorization_buf_len, _Bool flag)
{

	size_t olen = 0;

	char sign_buf[64];
	char hmac_sha1_buf[64];
	char access_key_base64[64];
	char string_for_signature[72];


	if(ver == (void *)0 || res == (void *)0 || et < 1564562581 || access_key == (void *)0
		|| authorization_buf == (void *)0 || authorization_buf_len < 120)
		return 1;


	// 第一步：把配置中的 Base64 密钥解码成 HMAC-SHA1 使用的原始密钥。
	memset(access_key_base64, 0, sizeof(access_key_base64));
	BASE64_Decode((unsigned char *)access_key_base64, sizeof(access_key_base64), &olen, (unsigned char *)access_key, strlen(access_key));


	// 第二步：根据是否面向产品或设备资源，生成待签名字符串。
	memset(string_for_signature, 0, sizeof(string_for_signature));
	if(flag)
		snprintf(string_for_signature, sizeof(string_for_signature), "%d\n%s\nproducts/%s\n%s", et, METHOD, res, ver);
	else
		snprintf(string_for_signature, sizeof(string_for_signature), "%d\n%s\nproducts/%s/devices/%s\n%s", et, METHOD, res, dev_name, ver);


	// 第三步：执行 HMAC-SHA1，再把签名结果 Base64 编码。
	memset(hmac_sha1_buf, 0, sizeof(hmac_sha1_buf));

	hmac_sha1((unsigned char *)access_key_base64, strlen(access_key_base64),
				(unsigned char *)string_for_signature, strlen(string_for_signature),
				(unsigned char *)hmac_sha1_buf);


	olen = 0;
	memset(sign_buf, 0, sizeof(sign_buf));
	BASE64_Encode((unsigned char *)sign_buf, sizeof(sign_buf), &olen, (unsigned char *)hmac_sha1_buf, strlen(hmac_sha1_buf));


	// 第四步：对签名结果进行 URL 编码，确保能安全放进 HTTP/MQTT 参数中。
	OTA_UrlEncode(sign_buf);


	// 第五步：拼出最终 Authorization 字符串。
	if(flag)
		snprintf(authorization_buf, authorization_buf_len, "version=%s&res=products%%2F%s&et=%d&method=%s&sign=%s", ver, res, et, METHOD, sign_buf);
	else
		snprintf(authorization_buf, authorization_buf_len, "version=%s&res=products%%2F%s%%2Fdevices%%2F%s&et=%d&method=%s&sign=%s", ver, res, dev_name, et, METHOD, sign_buf);


	return 0;

}


/*
 * 功能：通过 HTTP 接口在云端注册设备并获取设备凭据。
 * 运行流程：
 *   1. 建立到 OneNET HTTP 注册地址的 TCP 连接。
 *   2. 生成面向产品资源的 Authorization，拼出完整 HTTP POST 请求。
 *   3. 发送请求并等待返回，在响应体中提取 device_id 和 key。
 *   4. 关闭 TCP 连接并把结果返回给上层。
 */
_Bool OneNET_RegisterDevice(void)
{

	_Bool result = 1;
	unsigned short send_len = 11 + strlen(DEVICE_NAME);
	char *send_ptr = NULL, *data_ptr = NULL;

	char authorization_buf[144];

	send_ptr = malloc(send_len + 240);
	if(send_ptr == NULL)
		return result;

	while(ESP8266_SendCmd("AT+CIPSTART=\"TCP\",\"183.230.40.33\",80\r\n", "CONNECT"))
		delay_ms(500);

	OneNET_Authorization("2018-10-31", PROID, 1956499200, ACCESS_KEY, NULL,
							authorization_buf, sizeof(authorization_buf), 1);

	snprintf(send_ptr, 240 + send_len, "POST /mqtt/v1/devices/reg HTTP/1.1\r\n"
					"Authorization:%s\r\n"
					"Host:ota.heclouds.com\r\n"
					"Content-Type:application/json\r\n"
					"Content-Length:%d\r\n\r\n"
					"{\"name\":\"%s\"}",

					authorization_buf, 11 + strlen(DEVICE_NAME), DEVICE_NAME);

	ESP8266_SendData((unsigned char *)send_ptr, strlen(send_ptr));


	data_ptr = (char *)ESP8266_GetIPD(250);

	if(data_ptr)
	{
		data_ptr = strstr(data_ptr, "device_id");
	}

	if(data_ptr)
	{
		char name[16];
		int pid = 0;

		if(sscanf(data_ptr, "device_id\" : \"%[^\"]\",\r\n\"name\" : \"%[^\"]\",\r\n\r\n\"pid\" : %d,\r\n\"key\" : \"%[^\"]\"", devid, name, &pid, key) == 4)
		{
			UsartPrintf(USART_DEBUG, "create device: %s, %s, %d, %s\r\n", devid, name, pid, key);
			result = 0;
		}
	}

	free(send_ptr);
	ESP8266_SendCmd("AT+CIPCLOSE\r\n", "OK");

	return result;

}


/*
 * 功能：建立设备 MQTT 连接并校验 CONNACK 结果。
 * 运行流程：
 *   1. 先生成设备级 Authorization 字符串。
 *   2. 使用 MQTT_PacketConnect 组装 CONNECT 报文。
 *   3. 通过 ESP8266 发送到已经建立好的 TCP 连接。
 *   4. 等待 CONNACK 并根据返回码输出详细调试信息。
 */
_Bool OneNet_DevLink(void)
{

	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};

	unsigned char *dataPtr;

	char authorization_buf[160];

	_Bool status = 1;
	UsartPrintf(USART_DEBUG, "OneNET MQTT step1: auth\r\n");

	// 第一步：生成设备级 MQTT 鉴权参数。
	OneNET_Authorization("2018-10-31", PROID, 1956499200, ACCESS_KEY, DEVICE_NAME,
								authorization_buf, sizeof(authorization_buf), 0);
	UsartPrintf(USART_DEBUG, "OneNET MQTT step2: connect packet\r\n");

	UsartPrintf(USART_DEBUG, "OneNET_DevLink\r\n"
							"NAME: %s,	PROID: %s,	KEY:%s\r\n"
                        , DEVICE_NAME, PROID, authorization_buf);

	// 第二步：封装 MQTT CONNECT 报文并通过 ESP8266 发送。
	if(MQTT_PacketConnect(PROID, authorization_buf, DEVICE_NAME, 256, 1, MQTT_QOS_LEVEL0, NULL, NULL, 0, &mqttPacket) == 0)
	{
		UsartPrintf(USART_DEBUG, "OneNET MQTT step3: send connect\r\n");
		ESP8266_SendData(mqttPacket._data, mqttPacket._len);

		UsartPrintf(USART_DEBUG, "OneNET MQTT step4: wait connack\r\n");
		// 第三步：等待平台返回 CONNACK，并根据返回码判断链路是否建立成功。
		dataPtr = ESP8266_GetIPD(250);
		if(dataPtr != NULL)
		{
			unsigned char pktType = MQTT_UnPacketRecv(dataPtr);
			if(pktType == MQTT_PKT_CONNACK)
			{
				switch(MQTT_UnPacketConnectAck(dataPtr))
				{
					case 0:UsartPrintf(USART_DEBUG, "Tips:	连接成功\r\n");status = 0;break;

					case 1:UsartPrintf(USART_DEBUG, "WARN:	连接失败，协议错误\r\n");break;
					case 2:UsartPrintf(USART_DEBUG, "WARN:	连接失败，客户端ID无效\r\n");break;
					case 3:UsartPrintf(USART_DEBUG, "WARN:	连接失败，服务器内部错误\r\n");break;
					case 4:UsartPrintf(USART_DEBUG, "WARN:	连接失败，用户名或密码错误\r\n");break;
					case 5:UsartPrintf(USART_DEBUG, "WARN:	连接失败，认证信息错误\r\n");break;

					default:UsartPrintf(USART_DEBUG, "ERR:	连接失败，未知错误\r\n");break;
				}
			}
			else
			{
				UsartPrintf(USART_DEBUG, "WARN:\tMQTT recv not CONNACK, type=%d\r\n", pktType);
			}
		}
		else
		{
			UsartPrintf(USART_DEBUG, "WARN:\tMQTT connect timeout, no IPD data\r\n");
		}

		MQTT_DeleteBuffer(&mqttPacket);
	}
	else
		UsartPrintf(USART_DEBUG, "WARN:	MQTT_PacketConnect Failed\r\n");

	return status;

}

/*
 * 功能：组装属性上报 JSON 负载。
 * 说明：把当前全局传感器缓存映射成 OneNET 物模型属性字段，供上报函数直接发送。
 */
unsigned char OneNet_FillBuf(char *buf)
{

	char text[64];

	memset(text, 0, sizeof(text));

	strcpy(buf, "{\"id\":\"123\",\"params\":{");

	memset(text, 0, sizeof(text));
	sprintf(text, "\"temp\":{\"value\":%d},", temp);
	strcat(buf, text);

	memset(text, 0, sizeof(text));
	sprintf(text, "\"humi\":{\"value\":%d},", humi);
	strcat(buf, text);

	memset(text, 0, sizeof(text));
	sprintf(text, "\"mq2\":{\"value\":%d},", mq2Value);
	strcat(buf, text);

	memset(text, 0, sizeof(text));
	sprintf(text, "\"mq2_ppm\":{\"value\":%.2f},", mq2PpmValue);
	strcat(buf, text);

	memset(text, 0, sizeof(text));
	/* 上报布尔值：0 -> false（无人），1 -> true（有人）。 */
	sprintf(text, "\"sr501\":{\"value\":%s}", bodyValue ? "true" : "false");
	strcat(buf, text);

	strcat(buf, "}}");

	return strlen(buf);

}


/*
 * 功能：构造并发送 OneNET 属性上报报文。
 * 运行流程：
 *   1. 先调用 OneNet_FillBuf 生成 JSON 属性内容。
 *   2. 再利用 MQTT_PacketSaveData 构造属性上报主题和 MQTT 报文头。
 *   3. 把 JSON 正文追加到报文尾部后，通过 ESP8266 发送出去。
 */
void OneNet_SendData(void)
{

	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};

	char buf[256];

	short body_len = 0, i = 0;


	memset(buf, 0, sizeof(buf));

	body_len = OneNet_FillBuf(buf);

	if(body_len)
	{
		if(MQTT_PacketSaveData(PROID, DEVICE_NAME, body_len, NULL, &mqttPacket) == 0)
		{
			for(; i < body_len; i++)
				mqttPacket._data[mqttPacket._len++] = buf[i];

			ESP8266_SendData(mqttPacket._data, mqttPacket._len);


			MQTT_DeleteBuffer(&mqttPacket);
		}
		else
			UsartPrintf(USART_DEBUG, "WARN:	EDP_NewBuffer Failed\r\n");
	}

}


/* 功能：按指定主题发布一条 MQTT 消息，适合临时调试或扩展业务消息。 */
void OneNET_Publish(const char *topic, const char *msg)
{

	MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0};

	UsartPrintf(USART_DEBUG, "Publish Topic: %s, Msg: %s\r\n", topic, msg);

	if(MQTT_PacketPublish(MQTT_PUBLISH_ID, topic, msg, strlen(msg), MQTT_QOS_LEVEL0, 0, 1, &mqtt_packet) == 0)
	{
		ESP8266_SendData(mqtt_packet._data, mqtt_packet._len);

		MQTT_DeleteBuffer(&mqtt_packet);
	}

}


/*
 * 功能：订阅属性设置主题，接收平台下发命令。
 * 说明：当前订阅的是 thing/property/set 主题，用于接收平台侧属性设置请求。
 */
void OneNET_Subscribe(void)
{

	MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0};

	char topic_buf[56];
	const char *topic = topic_buf;

	snprintf(topic_buf, sizeof(topic_buf), "$sys/%s/%s/thing/property/set", PROID, DEVICE_NAME);

	UsartPrintf(USART_DEBUG, "Subscribe Topic: %s\r\n", topic_buf);

	if(MQTT_PacketSubscribe(MQTT_SUBSCRIBE_ID, MQTT_QOS_LEVEL0, &topic, 1, &mqtt_packet) == 0)
	{
		ESP8266_SendData(mqtt_packet._data, mqtt_packet._len);

		MQTT_DeleteBuffer(&mqtt_packet);
	}

}


/*
 * 功能：解析 OneNET 下行报文并处理 ACK/订阅应答。
 * 运行流程：
 *   1. 先识别当前报文类型。
 *   2. 对 PUBLISH 进一步拆出主题和负载，并打印调试信息。
 *   3. 对 PUBACK、SUBACK 分别给出发送成功或订阅结果提示。
 *   4. 处理完成后清空 ESP8266 接收缓存，并释放动态申请的主题/负载内存。
 */
void OneNet_RevPro(unsigned char *cmd)
{

	int8 *req_payload = NULL;
	int8 *cmdid_topic = NULL;

	unsigned short topic_len = 0;
	unsigned short req_len = 0;

	unsigned char qos = 0;
	static unsigned short pkt_id = 0;

	unsigned char type = 0;

	short result = 0;

	// 第一步：识别报文类型，决定后续进入哪条解析分支。
	type = MQTT_UnPacketRecv(cmd);
	switch(type)
	{
		case MQTT_PKT_PUBLISH:
			result = MQTT_UnPacketPublish(cmd, &cmdid_topic, &topic_len, &req_payload, &req_len, &qos, &pkt_id);
			if(result == 0)
			{
				UsartPrintf(USART_DEBUG, "topic: %s, topic_len: %d, payload: %s, payload_len: %d\r\n",
																	cmdid_topic, topic_len, req_payload, req_len);
			}
		break;

		case MQTT_PKT_PUBACK:

			if(MQTT_UnPacketPublishAck(cmd) == 0)
				UsartPrintf(USART_DEBUG, "Tips:	MQTT Publish Send OK\r\n");

		break;

		case MQTT_PKT_SUBACK:

			if(MQTT_UnPacketSubscribe(cmd) == 0)
				UsartPrintf(USART_DEBUG, "Tips:	MQTT Subscribe OK\r\n");
			else
				UsartPrintf(USART_DEBUG, "Tips:	MQTT Subscribe Err\r\n");

		break;

		default:
			result = -1;
		break;
	}

	ESP8266_Clear();

	if(result == -1)
		return;

	if(type == MQTT_PKT_CMD || type == MQTT_PKT_PUBLISH)
	{
		if(cmdid_topic != NULL)
			MQTT_FreeBuffer(cmdid_topic);
		if(req_payload != NULL)
			MQTT_FreeBuffer(req_payload);
	}

}
