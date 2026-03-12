/**
 * @file endianconv.h
 * @brief 字节序转换工具接口
 *        提供内存原地字节序翻转函数（memrev*）和整数字节序翻转函数（intrev*），
 *        以及按条件（仅大端机器执行）翻转的宏（*ifbe），
 *        以及主机字节序与网络字节序互转的 64 位宏（htonu64/ntohu64）。
 *
 * Copyright (c) 2011-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 * （详见 endianconv.c 顶部版权声明）
 */

#ifndef __ENDIANCONV_H
#define __ENDIANCONV_H

#include "utils/sys_config.h"
#include <stdint.h>

/**
 * @brief 原地翻转内存中 16 位数据的字节序
 * @param p 指向待翻转的 2 字节内存区域
 */
void memrev16(void *p);

/**
 * @brief 原地翻转内存中 32 位数据的字节序
 * @param p 指向待翻转的 4 字节内存区域
 */
void memrev32(void *p);

/**
 * @brief 原地翻转内存中 64 位数据的字节序
 * @param p 指向待翻转的 8 字节内存区域
 */
void memrev64(void *p);

/**
 * @brief 翻转 16 位整数的字节序并返回
 * @param v 原始 16 位整数
 * @return 字节序翻转后的值
 */
uint16_t intrev16(uint16_t v);

/**
 * @brief 翻转 32 位整数的字节序并返回
 * @param v 原始 32 位整数
 * @return 字节序翻转后的值
 */
uint32_t intrev32(uint32_t v);

/**
 * @brief 翻转 64 位整数的字节序并返回
 * @param v 原始 64 位整数
 * @return 字节序翻转后的值
 */
uint64_t intrev64(uint64_t v);

/* 仅在大端机器上执行翻转的宏；小端机器上为空操作 */
#if (BYTE_ORDER == LITTLE_ENDIAN)
/** @brief 小端机器：16 位内存翻转为空操作 */
#define memrev16ifbe(p) ((void)(0))
/** @brief 小端机器：32 位内存翻转为空操作 */
#define memrev32ifbe(p) ((void)(0))
/** @brief 小端机器：64 位内存翻转为空操作 */
#define memrev64ifbe(p) ((void)(0))
/** @brief 小端机器：16 位整数翻转为恒等 */
#define intrev16ifbe(v) (v)
/** @brief 小端机器：32 位整数翻转为恒等 */
#define intrev32ifbe(v) (v)
/** @brief 小端机器：64 位整数翻转为恒等 */
#define intrev64ifbe(v) (v)
#else
/** @brief 大端机器：原地翻转 16 位内存字节序 */
#define memrev16ifbe(p) memrev16(p)
/** @brief 大端机器：原地翻转 32 位内存字节序 */
#define memrev32ifbe(p) memrev32(p)
/** @brief 大端机器：原地翻转 64 位内存字节序 */
#define memrev64ifbe(p) memrev64(p)
/** @brief 大端机器：翻转 16 位整数字节序 */
#define intrev16ifbe(v) intrev16(v)
/** @brief 大端机器：翻转 32 位整数字节序 */
#define intrev32ifbe(v) intrev32(v)
/** @brief 大端机器：翻转 64 位整数字节序 */
#define intrev64ifbe(v) intrev64(v)
#endif

/**
 * htonu64 / ntohu64：主机字节序与网络字节序（大端）互转（64 位）
 * 大端机器上为空操作；小端机器上调用 intrev64 翻转。
 */
#if (BYTE_ORDER == BIG_ENDIAN)
/** @brief 大端机器：主机序转网络序（64 位），恒等 */
#define htonu64(v) (v)
/** @brief 大端机器：网络序转主机序（64 位），恒等 */
#define ntohu64(v) (v)
#else
/** @brief 小端机器：主机序转网络序（64 位），调用 intrev64 */
#define htonu64(v) intrev64(v)
/** @brief 小端机器：网络序转主机序（64 位），调用 intrev64 */
#define ntohu64(v) intrev64(v)
#endif



#endif
