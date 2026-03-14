/*
 * 模块说明：HMAC-SHA1 接口声明
 * 编码说明：本文件注释统一为 UTF-8。
 */

#ifndef _HAMC_SHA1_H_
#define _HAMC_SHA1_H_


#define MAX_MESSAGE_LENGTH		1024


/* 功能：计算输入数据的 HMAC-SHA1 摘要。 */
void hmac_sha1(
	unsigned char *key,
	int key_length,
	unsigned char *data,
	int data_length,
	unsigned char *digest
);


#endif
