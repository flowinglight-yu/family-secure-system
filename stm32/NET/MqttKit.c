/*
 * 模块说明：MQTT 报文封装与解析实现。
 * 设计目标：把 OneNET 业务侧需要的 CONNECT、PUBLISH、SUBSCRIBE、ACK 等报文
 *           拼装和拆包逻辑收敛到同一文件，便于上层只关注主题和负载内容。
 * 编码说明：本文件注释统一为 UTF-8。
 */

#include "MqttKit.h"


#include <string.h>
#include <stdio.h>


// OneNET 平台下发命令时使用的主题前缀，用于区分普通 PUBLISH 和命令消息。
#define CMD_TOPIC_PREFIX		"$creq"


/* 功能：按指定大小初始化 MQTT 报文缓存，兼容动态申请和外部静态缓存两种模式。 */
void MQTT_NewBuffer(MQTT_PACKET_STRUCTURE *mqttPacket, uint32 size)
{

	uint32 i = 0;

	if(mqttPacket->_data == NULL)
	{
		mqttPacket->_memFlag = MEM_FLAG_ALLOC;

		mqttPacket->_data = (uint8 *)MQTT_MallocBuffer(size);
		if(mqttPacket->_data != NULL)
		{
			mqttPacket->_len = 0;

			mqttPacket->_size = size;

			for(; i < mqttPacket->_size; i++)
				mqttPacket->_data[i] = 0;
		}
	}
	else
	{
		mqttPacket->_memFlag = MEM_FLAG_STATIC;

		for(; i < mqttPacket->_size; i++)
			mqttPacket->_data[i] = 0;

		mqttPacket->_len = 0;

		if(mqttPacket->_size < size)
			mqttPacket->_data = NULL;
	}

}


/* 功能：释放动态缓存并清空报文结构。 */
void MQTT_DeleteBuffer(MQTT_PACKET_STRUCTURE *mqttPacket)
{

	if(mqttPacket->_memFlag == MEM_FLAG_ALLOC)
		MQTT_FreeBuffer(mqttPacket->_data);

	mqttPacket->_data = NULL;
	mqttPacket->_len = 0;
	mqttPacket->_size = 0;
	mqttPacket->_memFlag = MEM_FLAG_NULL;

}

/*
 * 功能：按 MQTT 可变长度规则编码剩余长度字段。
 * 运行流程：
 *   1. 每次取剩余长度的低 7 位写入当前字节。
 *   2. 若后续仍有高位数据，则把当前字节最高位置 1 表示“后续还有字节”。
 *   3. 最多编码 4 个字节；若超过协议允许范围则返回失败。
 */
int32 MQTT_DumpLength(size_t len, uint8 *buf)
{

	int32 i = 0;

	for(i = 1; i <= 4; ++i)
	{
		*buf = len % 128;
		len >>= 7;
		if(len > 0)
		{
			*buf |= 128;
			++buf;
		}
		else
		{
			return i;
		}
	}

	return -1;
}

/*
 * 功能：解析 MQTT 可变长度字段。
 * 运行流程：
 *   1. 按字节读取剩余长度字段，每个字节只取低 7 位参与累加。
 *   2. 使用 multiplier 累乘 128 还原真实长度。
 *   3. 若连续字节数超出 MQTT 协议限制，则返回错误码。
 */
int32 MQTT_ReadLength(const uint8 *stream, int32 size, uint32 *len)
{

	int32 i;
	const uint8 *in = stream;
	uint32 multiplier = 1;

	*len = 0;
	for(i = 0; i < size; ++i)
	{
		*len += (in[i] & 0x7f) * multiplier;

		if(!(in[i] & 0x80))
		{
			return i + 1;
		}

		multiplier <<= 7;
		if(multiplier >= 2097152)
		{
			return -2;
		}
	}

	return -1;

}


/*
 * 功能：识别接收缓冲中的 MQTT 报文类型。
 * 运行流程：
 *   1. 先根据固定报头高 4 位获取基础报文类型。
 *   2. 若是 PUBLISH，则继续解析剩余长度和主题字段。
 *   3. 对命令主题额外标记为 MQTT_PKT_CMD，便于上层直接走命令处理分支。
 */
uint8 MQTT_UnPacketRecv(uint8 *dataPtr)
{

	uint8 status = 255;
	uint8 type = dataPtr[0] >> 4;

	if(type < 1 || type > 14)
		return status;

	if(type == MQTT_PKT_PUBLISH)
	{
		uint8 *msgPtr;
		uint32 remain_len = 0;

		msgPtr = dataPtr + MQTT_ReadLength(dataPtr + 1, 4, &remain_len) + 1;

		if(remain_len < 2 || dataPtr[0] & 0x01)
			return 255;

		if(remain_len < ((uint16)msgPtr[0] << 8 | msgPtr[1]) + 2)
			return 255;

		if(strstr((int8 *)msgPtr + 2, CMD_TOPIC_PREFIX) != NULL)
			status = MQTT_PKT_CMD;
		else
			status = MQTT_PKT_PUBLISH;
	}
	else
		status = type;

	return status;

}


/*
 * 功能：构造 CONNECT 报文并写入缓存。
 * 运行流程：
 *   1. 根据 clean session、遗嘱、用户名和密码生成连接标志位。
 *   2. 预估整个 CONNECT 报文长度，并为输出缓冲区分配空间。
 *   3. 依次写入固定报头、协议名、协议级别、连接标志、保活时间。
 *   4. 最后按 MQTT 字符串格式追加 client id、遗嘱主题/内容、用户名和密码。
 */
uint8 MQTT_PacketConnect(const int8 *user, const int8 *password, const int8 *devid,
						uint16 cTime, uint1 clean_session, uint1 qos,
						const int8 *will_topic, const int8 *will_msg, int32 will_retain,
						MQTT_PACKET_STRUCTURE *mqttPacket)
{

	uint8 flags = 0;
	uint8 will_topic_len = 0;
	uint16 total_len = 15;
	int16 len = 0, devid_len = strlen(devid);

	if(!devid)
		return 1;

	total_len += devid_len + 2;


	// 第一步：根据会话和遗嘱配置拼出 CONNECT 标志位。
	if(clean_session)
	{
		flags |= MQTT_CONNECT_CLEAN_SESSION;
	}


	if(will_topic)
	{
		flags |= MQTT_CONNECT_WILL_FLAG;
		will_topic_len = strlen(will_topic);
		total_len += 4 + will_topic_len + strlen(will_msg);
	}


	switch((unsigned char)qos)
	{
		case MQTT_QOS_LEVEL0:
			flags |= MQTT_CONNECT_WILL_QOS0;
		break;

		case MQTT_QOS_LEVEL1:
			flags |= (MQTT_CONNECT_WILL_FLAG | MQTT_CONNECT_WILL_QOS1);
		break;

		case MQTT_QOS_LEVEL2:
			flags |= (MQTT_CONNECT_WILL_FLAG | MQTT_CONNECT_WILL_QOS2);
		break;

		default:
		return 2;
	}


	if(will_retain)
	{
		flags |= (MQTT_CONNECT_WILL_FLAG | MQTT_CONNECT_WILL_RETAIN);
	}


	if(!user || !password)
	{
		return 3;
	}
	flags |= MQTT_CONNECT_USER_NAME | MQTT_CONNECT_PASSORD;

	total_len += strlen(user) + strlen(password) + 4;


	// 第二步：按预估长度申请缓存，并开始写固定报头和可变报头。
	MQTT_NewBuffer(mqttPacket, total_len);
	if(mqttPacket->_data == NULL)
		return 4;

	memset(mqttPacket->_data, 0, total_len);


	mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_CONNECT << 4;


	len = MQTT_DumpLength(total_len - 5, mqttPacket->_data + mqttPacket->_len);
	if(len < 0)
	{
		MQTT_DeleteBuffer(mqttPacket);
		return 5;
	}
	else
		mqttPacket->_len += len;


	mqttPacket->_data[mqttPacket->_len++] = 0;
	mqttPacket->_data[mqttPacket->_len++] = 4;
	mqttPacket->_data[mqttPacket->_len++] = 'M';
	mqttPacket->_data[mqttPacket->_len++] = 'Q';
	mqttPacket->_data[mqttPacket->_len++] = 'T';
	mqttPacket->_data[mqttPacket->_len++] = 'T';


	mqttPacket->_data[mqttPacket->_len++] = 4;


    mqttPacket->_data[mqttPacket->_len++] = flags;


	mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(cTime);
	mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(cTime);


	mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(devid_len);
	mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(devid_len);

	strncat((int8 *)mqttPacket->_data + mqttPacket->_len, devid, devid_len);
	mqttPacket->_len += devid_len;


	// 第三步：按 MQTT 字符串格式依次追加 client id、遗嘱、用户名和密码。
	if(flags & MQTT_CONNECT_WILL_FLAG)
	{
		unsigned short mLen = 0;

		if(!will_msg)
			will_msg = "";

		mLen = strlen(will_topic);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(mLen);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(mLen);
		strncat((int8 *)mqttPacket->_data + mqttPacket->_len, will_topic, mLen);
		mqttPacket->_len += mLen;

		mLen = strlen(will_msg);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(mLen);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(mLen);
		strncat((int8 *)mqttPacket->_data + mqttPacket->_len, will_msg, mLen);
		mqttPacket->_len += mLen;
	}


	if(flags & MQTT_CONNECT_USER_NAME)
	{
		unsigned short user_len = strlen(user);

		mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(user_len);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(user_len);
		strncat((int8 *)mqttPacket->_data + mqttPacket->_len, user, user_len);
		mqttPacket->_len += user_len;
	}


	if(flags & MQTT_CONNECT_PASSORD)
	{
		unsigned short psw_len = strlen(password);

		mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(psw_len);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(psw_len);
		strncat((int8 *)mqttPacket->_data + mqttPacket->_len, password, psw_len);
		mqttPacket->_len += psw_len;
	}

	return 0;

}


/* 功能：构造 DISCONNECT 报文。 */
uint1 MQTT_PacketDisConnect(MQTT_PACKET_STRUCTURE *mqttPacket)
{

	MQTT_NewBuffer(mqttPacket, 2);
	if(mqttPacket->_data == NULL)
		return 1;


	mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_DISCONNECT << 4;


	mqttPacket->_data[mqttPacket->_len++] = 0;

	return 0;

}


/* 功能：解析 CONNACK 并返回连接状态码，供上层判断鉴权结果。 */
uint8 MQTT_UnPacketConnectAck(uint8 *rev_data)
{

	if(rev_data[1] != 2)
		return 1;

	if(rev_data[2] == 0 || rev_data[2] == 1)
		return rev_data[3];
	else
		return 255;

}


/* 功能：构造属性上报主题的 PUBLISH 报文头，实际负载由上层继续拼接。 */
uint1 MQTT_PacketSaveData(const int8 *pro_id, const char *dev_name,
								int16 send_len, int8 *type_bin_head, MQTT_PACKET_STRUCTURE *mqttPacket)
{

	char topic_buf[48];

	snprintf(topic_buf, sizeof(topic_buf), "$sys/%s/%s/thing/property/post", pro_id, dev_name);

	if(MQTT_PacketPublish(MQTT_PUBLISH_ID, topic_buf, NULL, send_len + 0, MQTT_QOS_LEVEL1, 0, 1, mqttPacket) == 0)
	{


	}
	else
		return 1;

	return 0;

}


/*
 * 功能：构造二进制数据上报报文。
 * 运行流程：
 *   1. 先拼出 {"ds_id":"xxx"} 形式的二进制头描述。
 *   2. 再按项目约定的扩展格式组装 payload：类型字节 + 头长度 + 头内容 + 文件长度。
 *   3. 最后复用 MQTT_PacketPublish 统一生成 PUBLISH 报文。
 */
uint1 MQTT_PacketSaveBinData(const int8 *name, int16 file_len, MQTT_PACKET_STRUCTURE *mqttPacket)
{

	uint1 result = 1;
	int8 *bin_head = NULL;
	uint8 bin_head_len = 0;
	int8 *payload = NULL;
	int32 payload_size = 0;

	bin_head = (int8 *)MQTT_MallocBuffer(13 + strlen(name));
	if(bin_head == NULL)
		return result;

	sprintf(bin_head, "{\"ds_id\":\"%s\"}", name);

	bin_head_len = strlen(bin_head);
	payload_size = 7 + bin_head_len + file_len;

	payload = (int8 *)MQTT_MallocBuffer(payload_size - file_len);
	if(payload == NULL)
	{
		MQTT_FreeBuffer(bin_head);

		return result;
	}

	payload[0] = 2;

	payload[1] = MOSQ_MSB(bin_head_len);
	payload[2] = MOSQ_LSB(bin_head_len);

	memcpy(payload + 3, bin_head, bin_head_len);

	payload[bin_head_len + 3] = (file_len >> 24) & 0xFF;
	payload[bin_head_len + 4] = (file_len >> 16) & 0xFF;
	payload[bin_head_len + 5] = (file_len >> 8) & 0xFF;
	payload[bin_head_len + 6] = file_len & 0xFF;

	if(MQTT_PacketPublish(MQTT_PUBLISH_ID, "$dp", payload, payload_size, MQTT_QOS_LEVEL1, 0, 1, mqttPacket) == 0)
		result = 0;

	MQTT_FreeBuffer(bin_head);
	MQTT_FreeBuffer(payload);

	return result;

}


/*
 * 功能：从命令主题中提取 cmdid 与请求体。
 * 运行流程：
 *   1. 先在主题路径中找到 cmdid 起始位置。
 *   2. 再结合 MQTT 剩余长度字段计算请求体长度。
 *   3. 分别为 cmdid 和请求体申请新缓冲区并拷贝出去。
 */
uint8 MQTT_UnPacketCmd(uint8 *rev_data, int8 **cmdid, int8 **req, uint16 *req_len)
{

	int8 *dataPtr = strchr((int8 *)rev_data + 6, '/');

	uint32 remain_len = 0;

	if(dataPtr == NULL)
		return 1;
	dataPtr++;

	MQTT_ReadLength(rev_data + 1, 4, &remain_len);

	*cmdid = (int8 *)MQTT_MallocBuffer(37);
	if(*cmdid == NULL)
		return 2;

	memset(*cmdid, 0, 37);
	memcpy(*cmdid, (const int8 *)dataPtr, 36);
	dataPtr += 36;

	*req_len = remain_len - 44;
	*req = (int8 *)MQTT_MallocBuffer(*req_len + 1);
	if(*req == NULL)
	{
		MQTT_FreeBuffer(*cmdid);
		return 3;
	}

	memset(*req, 0, *req_len + 1);
	memcpy(*req, (const int8 *)dataPtr, *req_len);

	return 0;

}


/* 功能：构造命令响应 PUBLISH 报文。 */
uint1 MQTT_PacketCmdResp(const int8 *cmdid, const int8 *req, MQTT_PACKET_STRUCTURE *mqttPacket)
{

	uint16 cmdid_len = strlen(cmdid);
	uint16 req_len = strlen(req);
	_Bool status = 0;

	int8 *payload = MQTT_MallocBuffer(cmdid_len + 7);
	if(payload == NULL)
		return 1;

	memset(payload, 0, cmdid_len + 7);
	memcpy(payload, "$crsp/", 6);
	strncat(payload, cmdid, cmdid_len);

	if(MQTT_PacketPublish(MQTT_PUBLISH_ID, payload, req, strlen(req), MQTT_QOS_LEVEL0, 0, 1, mqttPacket) == 0)
		status = 0;
	else
		status = 1;

	MQTT_FreeBuffer(payload);

	return status;

}


/* 功能：构造 SUBSCRIBE 报文，按“报文标识符 + 多个主题 + QoS”顺序编码。 */
uint8 MQTT_PacketSubscribe(uint16 pkt_id, enum MqttQosLevel qos, const int8 *topics[], uint8 topics_cnt, MQTT_PACKET_STRUCTURE *mqttPacket)
{

	uint32 topic_len = 0, remain_len = 0;
	int16 len = 0;
	uint8 i = 0;

	if(pkt_id == 0)
		return 1;


	for(; i < topics_cnt; i++)
	{
		if(topics[i] == NULL)
			return 2;

		topic_len += strlen(topics[i]);
	}


	remain_len = 2 + 3 * topics_cnt + topic_len;


	MQTT_NewBuffer(mqttPacket, remain_len + 5);
	if(mqttPacket->_data == NULL)
		return 3;


	mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_SUBSCRIBE << 4 | 0x02;


	len = MQTT_DumpLength(remain_len, mqttPacket->_data + mqttPacket->_len);
	if(len < 0)
	{
		MQTT_DeleteBuffer(mqttPacket);
		return 4;
	}
	else
		mqttPacket->_len += len;


	mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(pkt_id);
	mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(pkt_id);


	for(i = 0; i < topics_cnt; i++)
	{
		topic_len = strlen(topics[i]);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(topic_len);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(topic_len);

		strncat((int8 *)mqttPacket->_data + mqttPacket->_len, topics[i], topic_len);
		mqttPacket->_len += topic_len;

		mqttPacket->_data[mqttPacket->_len++] = qos & 0xFF;
	}

	return 0;

}


/* 功能：解析 SUBACK 报文。 */
uint8 MQTT_UnPacketSubscribe(uint8 *rev_data)
{

	uint8 result = 255;

	if(rev_data[2] == MOSQ_MSB(MQTT_SUBSCRIBE_ID) && rev_data[3] == MOSQ_LSB(MQTT_SUBSCRIBE_ID))
	{
		switch(rev_data[4])
		{
			case 0x00:
			case 0x01:
			case 0x02:

				result = 0;
			break;

			case 0x80:

				result = 1;
			break;

			default:

				result = 2;
			break;
		}
	}

	return result;

}


/* 功能：构造 UNSUBSCRIBE 报文，按 MQTT 规范写入待退订主题列表。 */
uint8 MQTT_PacketUnSubscribe(uint16 pkt_id, const int8 *topics[], uint8 topics_cnt, MQTT_PACKET_STRUCTURE *mqttPacket)
{

	uint32 topic_len = 0, remain_len = 0;
	int16 len = 0;
	uint8 i = 0;

	if(pkt_id == 0)
		return 1;


	for(; i < topics_cnt; i++)
	{
		if(topics[i] == NULL)
			return 2;

		topic_len += strlen(topics[i]);
	}


	remain_len = 2 + (topics_cnt << 1) + topic_len;


	MQTT_NewBuffer(mqttPacket, remain_len + 5);
	if(mqttPacket->_data == NULL)
		return 3;


	mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_UNSUBSCRIBE << 4 | 0x02;


	len = MQTT_DumpLength(remain_len, mqttPacket->_data + mqttPacket->_len);
	if(len < 0)
	{
		MQTT_DeleteBuffer(mqttPacket);
		return 4;
	}
	else
		mqttPacket->_len += len;


	mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(pkt_id);
	mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(pkt_id);


	for(i = 0; i < topics_cnt; i++)
	{
		topic_len = strlen(topics[i]);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(topic_len);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(topic_len);

		strncat((int8 *)mqttPacket->_data + mqttPacket->_len, topics[i], topic_len);
		mqttPacket->_len += topic_len;
	}

	return 0;

}


/* 功能：解析 UNSUBACK 报文。 */
uint1 MQTT_UnPacketUnSubscribe(uint8 *rev_data)
{

	uint1 result = 1;

	if(rev_data[2] == MOSQ_MSB(MQTT_UNSUBSCRIBE_ID) && rev_data[3] == MOSQ_LSB(MQTT_UNSUBSCRIBE_ID))
	{
		result = 0;
	}

	return result;

}


/*
 * 功能：构造通用 PUBLISH 报文。
 * 运行流程：
 *   1. 先校验 topic 与 QoS，计算固定报头和可变报头需要的总长度。
 *   2. 若 payload[0] == 2，说明是项目扩展的二进制负载，只拷贝约定头部和长度信息。
 *   3. 写入固定报头、剩余长度、主题名和可选的 packet id。
 *   4. 最后把普通负载或二进制负载拷贝到报文尾部。
 */
uint8 MQTT_PacketPublish(uint16 pkt_id, const int8 *topic,
						const int8 *payload, uint32 payload_len,
						enum MqttQosLevel qos, int32 retain, int32 own,
						MQTT_PACKET_STRUCTURE *mqttPacket)
{

	uint32 total_len = 0, topic_len = 0;
	uint32 data_len = 0;
	int32 len = 0;
	uint8 flags = 0;


	if(pkt_id == 0)
		return 1;


	// 第一步：校验主题是否合法，MQTT 普通主题中不能直接包含通配符。
	for(topic_len = 0; topic[topic_len] != '\0'; ++topic_len)
	{
		if((topic[topic_len] == '#') || (topic[topic_len] == '+'))
			return 2;
	}


	flags |= MQTT_PKT_PUBLISH << 4;


	if(retain)
		flags |= 0x01;


	total_len = topic_len + payload_len + 2;


	switch(qos)
	{
		case MQTT_QOS_LEVEL0:
			flags |= MQTT_CONNECT_WILL_QOS0;
		break;

		case MQTT_QOS_LEVEL1:
			flags |= 0x02;
			total_len += 2;
		break;

		case MQTT_QOS_LEVEL2:
			flags |= 0x04;
			total_len += 2;
		break;

		default:
		return 3;
	}


	// 第二步：根据负载类型申请缓存。普通负载直接按总长度分配，
	// 二进制负载只保留扩展头和文件长度描述部分。
	if(payload != NULL)
	{
		if(payload[0] == 2)
		{
			uint32 data_len_t = 0;

			while(payload[data_len_t++] != '}');
			data_len_t -= 3;
			data_len = data_len_t + 7;
			data_len_t = payload_len - data_len;

			MQTT_NewBuffer(mqttPacket, total_len + 3 - data_len_t);

			if(mqttPacket->_data == NULL)
				return 4;

			memset(mqttPacket->_data, 0, total_len + 3 - data_len_t);
		}
		else
		{
			MQTT_NewBuffer(mqttPacket, total_len + 5);

			if(mqttPacket->_data == NULL)
				return 4;

			memset(mqttPacket->_data, 0, total_len + 5);
		}
	}
	else
	{
		MQTT_NewBuffer(mqttPacket, total_len + 5);

		if(mqttPacket->_data == NULL)
			return 4;

		memset(mqttPacket->_data, 0, total_len + 5);
	}


	// 第三步：写入固定报头、剩余长度和主题名。
	mqttPacket->_data[mqttPacket->_len++] = flags;


	len = MQTT_DumpLength(total_len, mqttPacket->_data + mqttPacket->_len);
	if(len < 0)
	{
		MQTT_DeleteBuffer(mqttPacket);
		return 5;
	}
	else
		mqttPacket->_len += len;


	mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(topic_len);
	mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(topic_len);

	strncat((int8 *)mqttPacket->_data + mqttPacket->_len, topic, topic_len);
	mqttPacket->_len += topic_len;
	if(qos != MQTT_QOS_LEVEL0)
	{
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(pkt_id);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(pkt_id);
	}


	// 第四步：补齐 packet id，并把负载写入报文尾部。
	if(payload != NULL)
	{
		if(payload[0] == 2)
		{
			memcpy((int8 *)mqttPacket->_data + mqttPacket->_len, payload, data_len);
			mqttPacket->_len += data_len;
		}
		else
		{
			memcpy((int8 *)mqttPacket->_data + mqttPacket->_len, payload, payload_len);
			mqttPacket->_len += payload_len;
		}
	}

	return 0;

}


/*
 * 功能：解析 PUBLISH 报文中的主题、负载与 QoS 信息。
 * 运行流程：
 *   1. 先从固定报头中取出 dup 和 QoS 位。
 *   2. 再根据剩余长度定位主题字段，并判定是否属于命令主题。
 *   3. 按不同 QoS 决定是否需要额外读取 packet id。
 *   4. 为输出主题和负载单独申请缓存，方便上层直接使用。
 */
uint8 MQTT_UnPacketPublish(uint8 *rev_data, int8 **topic, uint16 *topic_len, int8 **payload, uint16 *payload_len, uint8 *qos, uint16 *pkt_id)
{

	const int8 flags = rev_data[0] & 0x0F;
	uint8 *msgPtr;
	uint32 remain_len = 0;

	const int8 dup = flags & 0x08;

	*qos = (flags & 0x06) >> 1;

	msgPtr = rev_data + MQTT_ReadLength(rev_data + 1, 4, &remain_len) + 1;

	if(remain_len < 2 || flags & 0x01)
		return 255;

	*topic_len = (uint16)msgPtr[0] << 8 | msgPtr[1];
	if(remain_len < *topic_len + 2)
		return 255;

	if(strstr((int8 *)msgPtr + 2, CMD_TOPIC_PREFIX) != NULL)
		return MQTT_PKT_CMD;

	switch(*qos)
	{
		case MQTT_QOS_LEVEL0:

			if(0 != dup)
				return 255;

			*topic = MQTT_MallocBuffer(*topic_len + 1);
			if(*topic == NULL)
				return 255;

			memset(*topic, 0, *topic_len + 1);
			memcpy(*topic, (int8 *)msgPtr + 2, *topic_len);

			*payload_len = remain_len - 2 - *topic_len;
			*payload = MQTT_MallocBuffer(*payload_len + 1);
			if(*payload == NULL)
			{
				MQTT_FreeBuffer(*topic);
				return 255;
			}

			memset(*payload, 0, *payload_len + 1);
			memcpy(*payload, (int8 *)msgPtr + 2 + *topic_len, *payload_len);

		break;

		case MQTT_QOS_LEVEL1:
		case MQTT_QOS_LEVEL2:

			if(*topic_len + 2 > remain_len)
				return 255;

			*pkt_id = (uint16)msgPtr[*topic_len + 2] << 8 | msgPtr[*topic_len + 3];
			if(pkt_id == 0)
				return 255;

			*topic = MQTT_MallocBuffer(*topic_len + 1);
			if(*topic == NULL)
				return 255;

			memset(*topic, 0, *topic_len + 1);
			memcpy(*topic, (int8 *)msgPtr + 2, *topic_len);

			*payload_len = remain_len - 4 - *topic_len;
			*payload = MQTT_MallocBuffer(*payload_len + 1);
			if(*payload == NULL)
			{
				MQTT_FreeBuffer(*topic);
				return 255;
			}

			memset(*payload, 0, *payload_len + 1);
			memcpy(*payload, (int8 *)msgPtr + 4 + *topic_len, *payload_len);

		break;

		default:
			return 255;
	}

	if(strchr((int8 *)topic, '+') || strchr((int8 *)topic, '#'))
		return 255;

	return 0;

}


/* 功能：构造 PUBACK 报文。 */
uint1 MQTT_PacketPublishAck(uint16 pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket)
{

	MQTT_NewBuffer(mqttPacket, 4);
	if(mqttPacket->_data == NULL)
		return 1;


	mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_PUBACK << 4;


	mqttPacket->_data[mqttPacket->_len++] = 2;


	mqttPacket->_data[mqttPacket->_len++] = pkt_id >> 8;
	mqttPacket->_data[mqttPacket->_len++] = pkt_id & 0xff;

	return 0;

}


/* 功能：解析 PUBACK 报文。 */
uint1 MQTT_UnPacketPublishAck(uint8 *rev_data)
{

	if(rev_data[1] != 2)
		return 1;

	if(rev_data[2] == MOSQ_MSB(MQTT_PUBLISH_ID) && rev_data[3] == MOSQ_LSB(MQTT_PUBLISH_ID))
		return 0;
	else
		return 1;

}


/* 功能：构造 PUBREC 报文。 */
uint1 MQTT_PacketPublishRec(uint16 pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket)
{

	MQTT_NewBuffer(mqttPacket, 4);
	if(mqttPacket->_data == NULL)
		return 1;


	mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_PUBREC << 4;


	mqttPacket->_data[mqttPacket->_len++] = 2;


	mqttPacket->_data[mqttPacket->_len++] = pkt_id >> 8;
	mqttPacket->_data[mqttPacket->_len++] = pkt_id & 0xff;

	return 0;

}


/* 功能：解析 PUBREC 报文。 */
uint1 MQTT_UnPacketPublishRec(uint8 *rev_data)
{

	if(rev_data[1] != 2)
		return 1;

	if(rev_data[2] == MOSQ_MSB(MQTT_PUBLISH_ID) && rev_data[3] == MOSQ_LSB(MQTT_PUBLISH_ID))
		return 0;
	else
		return 1;

}


/* 功能：构造 PUBREL 报文。 */
uint1 MQTT_PacketPublishRel(uint16 pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket)
{

	MQTT_NewBuffer(mqttPacket, 4);
	if(mqttPacket->_data == NULL)
		return 1;


	mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_PUBREL << 4 | 0x02;


	mqttPacket->_data[mqttPacket->_len++] = 2;


	mqttPacket->_data[mqttPacket->_len++] = pkt_id >> 8;
	mqttPacket->_data[mqttPacket->_len++] = pkt_id & 0xff;

	return 0;

}


/* 功能：解析 PUBREL 报文。 */
uint1 MQTT_UnPacketPublishRel(uint8 *rev_data, uint16 pkt_id)
{

	if(rev_data[1] != 2)
		return 1;

	if(rev_data[2] == MOSQ_MSB(pkt_id) && rev_data[3] == MOSQ_LSB(pkt_id))
		return 0;
	else
		return 1;

}


/* 功能：构造 PUBCOMP 报文。 */
uint1 MQTT_PacketPublishComp(uint16 pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket)
{

	MQTT_NewBuffer(mqttPacket, 4);
	if(mqttPacket->_data == NULL)
		return 1;


	mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_PUBCOMP << 4;


	mqttPacket->_data[mqttPacket->_len++] = 2;


	mqttPacket->_data[mqttPacket->_len++] = pkt_id >> 8;
	mqttPacket->_data[mqttPacket->_len++] = pkt_id & 0xff;

	return 0;

}


/* 功能：解析 PUBCOMP 报文。 */
uint1 MQTT_UnPacketPublishComp(uint8 *rev_data)
{

	if(rev_data[1] != 2)
		return 1;

	if(rev_data[2] == MOSQ_MSB(MQTT_PUBLISH_ID) && rev_data[3] == MOSQ_LSB(MQTT_PUBLISH_ID))
		return 0;
	else
		return 1;

}


/* 功能：构造 PINGREQ 心跳报文。 */
uint1 MQTT_PacketPing(MQTT_PACKET_STRUCTURE *mqttPacket)
{

	MQTT_NewBuffer(mqttPacket, 2);
	if(mqttPacket->_data == NULL)
		return 1;


	mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_PINGREQ << 4;


	mqttPacket->_data[mqttPacket->_len++] = 0;

	return 0;

}
