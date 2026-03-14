/*
 * 模块说明：MQTT 报文封装与解析接口声明。
 * 设计目标：向上层屏蔽 MQTT 固定报头、剩余长度、主题和负载拼装细节。
 * 使用边界：本文件只提供报文编解码能力，不负责底层网络收发和重连控制。
 * 编码说明：本文件注释统一为 UTF-8。
 */

#ifndef _MQTTKIT_H_
#define _MQTTKIT_H_


#include "Common.h"


#include <stdlib.h>

#define MQTT_MallocBuffer	malloc

#define MQTT_FreeBuffer		free


#define MOSQ_MSB(A)         (uint8)((A & 0xFF00) >> 8)
#define MOSQ_LSB(A)         (uint8)(A & 0x00FF)


#define MEM_FLAG_NULL		0
#define MEM_FLAG_ALLOC		1
#define MEM_FLAG_STATIC		2


typedef struct Buffer
{

	uint8	*_data;		// 报文缓存首地址。

	uint32	_len;		// 当前已写入的有效字节数。

	uint32	_size;		// 缓冲区总容量。

	uint8	_memFlag;	// 缓冲区内存来源：空、动态申请或静态缓冲。

} MQTT_PACKET_STRUCTURE;


enum MqttPacketType
{

    MQTT_PKT_CONNECT = 1,
    MQTT_PKT_CONNACK,
    MQTT_PKT_PUBLISH,
    MQTT_PKT_PUBACK,
    MQTT_PKT_PUBREC,
    MQTT_PKT_PUBREL,
    MQTT_PKT_PUBCOMP,
    MQTT_PKT_SUBSCRIBE,
    MQTT_PKT_SUBACK,
    MQTT_PKT_UNSUBSCRIBE,
    MQTT_PKT_UNSUBACK,
    MQTT_PKT_PINGREQ,
    MQTT_PKT_PINGRESP,
    MQTT_PKT_DISCONNECT,


	MQTT_PKT_CMD

};


enum MqttQosLevel
{

    MQTT_QOS_LEVEL0,
    MQTT_QOS_LEVEL1,
    MQTT_QOS_LEVEL2

};


enum MqttConnectFlag
{

    MQTT_CONNECT_CLEAN_SESSION  = 0x02,
    MQTT_CONNECT_WILL_FLAG      = 0x04,
    MQTT_CONNECT_WILL_QOS0      = 0x00,
    MQTT_CONNECT_WILL_QOS1      = 0x08,
    MQTT_CONNECT_WILL_QOS2      = 0x10,
    MQTT_CONNECT_WILL_RETAIN    = 0x20,
    MQTT_CONNECT_PASSORD        = 0x40,
    MQTT_CONNECT_USER_NAME      = 0x80

};


#define MQTT_PUBLISH_ID			10

#define MQTT_SUBSCRIBE_ID		20

#define MQTT_UNSUBSCRIBE_ID		30


/* 功能：释放 MQTT 报文缓冲资源并复位结构体状态。 */
void MQTT_DeleteBuffer(MQTT_PACKET_STRUCTURE *mqttPacket);


/* 功能：识别接收报文类型，PUBLISH 还会继续区分普通消息和命令主题。 */
uint8 MQTT_UnPacketRecv(uint8 *dataPtr);


/* 功能：按 MQTT 3.1.1 规则封装 CONNECT 报文，包含客户端标识、遗嘱、用户名和密码。 */
uint8 MQTT_PacketConnect(const int8 *user, const int8 *password, const int8 *devid,
						uint16 cTime, uint1 clean_session, uint1 qos,
						const int8 *will_topic, const int8 *will_msg, int32 will_retain,
						MQTT_PACKET_STRUCTURE *mqttPacket);


/* 功能：封装 DISCONNECT 报文。 */
uint1 MQTT_PacketDisConnect(MQTT_PACKET_STRUCTURE *mqttPacket);


/* 功能：解析 CONNACK 报文并返回平台给出的连接结果码。 */
uint8 MQTT_UnPacketConnectAck(uint8 *rev_data);


/* 功能：封装 OneNET 属性上报主题对应的 PUBLISH 报文头。 */
uint1 MQTT_PacketSaveData(const int8 *pro_id, const char *dev_name,
								int16 send_len, int8 *type_bin_head, MQTT_PACKET_STRUCTURE *mqttPacket);


/* 功能：封装二进制数据上报报文。 */
uint1 MQTT_PacketSaveBinData(const int8 *name, int16 file_len, MQTT_PACKET_STRUCTURE *mqttPacket);


/* 功能：从平台命令报文中拆出 cmdid 和请求体，供上层业务进一步处理。 */
uint8 MQTT_UnPacketCmd(uint8 *rev_data, int8 **cmdid, int8 **req, uint16 *req_len);


/* 功能：封装命令响应报文。 */
uint1 MQTT_PacketCmdResp(const int8 *cmdid, const int8 *req, MQTT_PACKET_STRUCTURE *mqttPacket);


/* 功能：封装 SUBSCRIBE 报文，可一次订阅多个主题。 */
uint8 MQTT_PacketSubscribe(uint16 pkt_id, enum MqttQosLevel qos, const int8 *topics[], uint8 topics_cnt, MQTT_PACKET_STRUCTURE *mqttPacket);


/* 功能：解析 SUBACK 报文。 */
uint8 MQTT_UnPacketSubscribe(uint8 *rev_data);


/* 功能：封装 UNSUBSCRIBE 报文，可一次取消多个主题。 */
uint8 MQTT_PacketUnSubscribe(uint16 pkt_id, const int8 *topics[], uint8 topics_cnt, MQTT_PACKET_STRUCTURE *mqttPacket);


/* 功能：解析 UNSUBACK 报文。 */
uint1 MQTT_UnPacketUnSubscribe(uint8 *rev_data);


/* 功能：封装 PUBLISH 报文，支持普通文本负载和项目中的二进制扩展负载。 */
uint8 MQTT_PacketPublish(uint16 pkt_id, const int8 *topic,
						const int8 *payload, uint32 payload_len,
						enum MqttQosLevel qos, int32 retain, int32 own,
						MQTT_PACKET_STRUCTURE *mqttPacket);


/* 功能：解析 PUBLISH 报文中的主题、负载、QoS 和报文标识符。 */
uint8 MQTT_UnPacketPublish(uint8 *rev_data, int8 **topic, uint16 *topic_len, int8 **payload, uint16 *payload_len, uint8 *qos, uint16 *pkt_id);


/* 功能：封装 PUBACK 报文。 */
uint1 MQTT_PacketPublishAck(uint16 pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket);


/* 功能：解析 PUBACK 报文。 */
uint1 MQTT_UnPacketPublishAck(uint8 *rev_data);


/* 功能：封装 PUBREC 报文。 */
uint1 MQTT_PacketPublishRec(uint16 pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket);


/* 功能：解析 PUBREC 报文。 */
uint1 MQTT_UnPacketPublishRec(uint8 *rev_data);


/* 功能：封装 PUBREL 报文。 */
uint1 MQTT_PacketPublishRel(uint16 pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket);


/* 功能：解析 PUBREL 报文。 */
uint1 MQTT_UnPacketPublishRel(uint8 *rev_data, uint16 pkt_id);


/* 功能：封装 PUBCOMP 报文。 */
uint1 MQTT_PacketPublishComp(uint16 pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket);


/* 功能：解析 PUBCOMP 报文。 */
uint1 MQTT_UnPacketPublishComp(uint8 *rev_data);


/* 功能：封装 PINGREQ 心跳报文。 */
uint1 MQTT_PacketPing(MQTT_PACKET_STRUCTURE *mqttPacket);


#endif
