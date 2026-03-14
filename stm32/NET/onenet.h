/*
 * 模块说明：OneNET 连接、订阅与上报接口声明。
 * 使用边界：本模块负责鉴权参数生成、MQTT 建链、属性上报和下行报文处理。
 * 编码说明：本文件注释统一为 UTF-8。
 */

#ifndef _ONENET_H_
#define _ONENET_H_


#ifndef ONENET_PRODUCT_ID
#define ONENET_PRODUCT_ID	""
#endif

#ifndef ONENET_DEVICE_NAME
#define ONENET_DEVICE_NAME	""
#endif
/* 注意：这里的 access key 是设备密钥，不是产品密钥。 */
#ifndef ONENET_ACCESS_KEY
#define ONENET_ACCESS_KEY	""
#endif

/* 功能：在 OneNET 平台侧创建设备（按需使用）。 */
_Bool OneNET_RegisterDevice(void);

/* 功能：发起 MQTT 鉴权并建立设备链路，成功后可继续订阅和上报。 */
_Bool OneNet_DevLink(void);

/* 功能：打包并上报当前传感器属性数据。 */
void OneNet_SendData(void);

/* 功能：订阅 OneNET 属性下发主题。 */
void OneNET_Subscribe(void);

/* 功能：解析并处理 OneNET 下行 MQTT 报文，包括发布应答和订阅应答。 */
void OneNet_RevPro(unsigned char *cmd);

#endif
