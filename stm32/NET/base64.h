/*
 * 模块说明：Base64 编解码接口声明
 * 编码说明：本文件注释统一为 UTF-8。
 */

#ifndef _BASE64_H
#define _BASE64_H

#include <stddef.h>

#define MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL               -0x002A
#define MBEDTLS_ERR_BASE64_INVALID_CHARACTER              -0x002C

#ifdef __cplusplus
extern "C" {
#endif


/* 功能：将二进制数据编码为 Base64 字符串。 */
int BASE64_Encode(unsigned char *dst, size_t dlen, size_t *olen,
						  const unsigned char *src, size_t slen);

/* 功能：将 Base64 字符串解码为二进制数据。 */
int BASE64_Decode(unsigned char *dst, size_t dlen, size_t *olen,
						  const unsigned char *src, size_t slen);


#ifdef __cplusplus
}
#endif

#endif
