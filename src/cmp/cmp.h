/*
 * cmp.h - 比较函数集头文件
 * 
 * Latte C 库工具组件：通用比较函数集合
 * 
 * 核心功能：
 * 1. 类型化比较函数 (int/uint/string/pointer)
 * 2. 大小端相关比较
 * 3. 字符串比较 (区分/不区分大小写)
 * 4. 浮点数比较 (支持容差)
 * 
 * 应用场景：
 * - 哈希表的 keyCompare 回调
 * - 排序算法的比较器
 * - 缓存的键比较
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

#ifndef __LATTE_CMP_H
#define __LATTE_CMP_H

#include <string.h>
#include <stdint.h>
#include <float.h>

/* 整数比较函数 */
int cmp_int(void *a, void *b);
int cmp_uint(void *a, void *b);
int cmp_long(void *a, void *b);
int cmp_ulong(void *a, void *b);

/* 64 位整数比较 */
int cmp_int64(void *a, void *b);
int cmp_uint64(void *a, void *b);

/* 字符串比较 */
int cmp_string(void *a, void *b);          /* 区分大小写 */
int cmp_string_nocase(void *a, void *b);   /* 不区分大小写 */

/* 指针比较 (直接比较地址) */
int cmp_pointer(void *a, void *b);

/* 浮点数比较 */
int cmp_double(void *a, void *b);
int cmp_float(void *a, void *b);

/* 二进制数据比较 */
int cmp_binary(void *a, void *b, size_t len);

/* 字节序相关比较 */
int cmp_uint16_be(void *a, void *b);  /* 大端序 */
int cmp_uint32_be(void *a, void *b);
int cmp_uint64_be(void *a, void *b);

int cmp_uint16_le(void *a, void *b);  /* 小端序 */
int cmp_uint32_le(void *a, void *b);
int cmp_uint64_le(void *a, void *b);

/* 比较函数返回约定 */
#define CMP_LT (-1)  /* a < b */
#define CMP_EQ 0      /* a == b */
#define CMP_GT 1      /* a > b */

#endif /* __LATTE_CMP_H */
