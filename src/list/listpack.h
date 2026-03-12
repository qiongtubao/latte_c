/**
 * @file listpack.h
 * @brief 紧凑型列表包（listpack）接口
 *        listpack 是一种紧凑的序列化格式，将多个字符串/整数元素连续存储在单块内存中，
 *        支持从头部或尾部高效插入，适合小数据量场景（如 hash、zset 的紧凑表示）。
 *        内存布局：[total_bytes(4B)][num_elements(2B)][entry...][end(1B, 0xFF)]
 */
//
// Created by dong on 23-5-22.
//

#ifndef LATTE_C_LISTPACK_H
#define LATTE_C_LISTPACK_H

/** @brief lpInsert() 位置参数：在目标元素之前插入 */
#define LP_BEFORE 0
/** @brief lpInsert() 位置参数：在目标元素之后插入 */
#define LP_AFTER 1
/** @brief lpInsert() 位置参数：替换目标元素 */
#define LP_REPLACE 2

#include <assert.h>
#include "zmalloc.h"
#include "utils/utils.h"


/** @brief 整数字符串缓冲区大小：-2^63 的最大位数(20) + 1 个 null 终止符 */
#define LP_INTBUF_SIZE 21 /* 20 digits of -2^63 + 1 null term = 21. */

/* 使用 zmalloc_usable/zrealloc_usable 以支持 zmalloc_usable_size() 的安全调用 */
/** @brief listpack 内存分配宏（zmalloc_usable，返回实际分配大小） */
#define lp_malloc(sz) zmalloc_usable(sz,NULL)
/** @brief listpack 内存重新分配宏 */
#define lp_realloc(ptr,sz) zrealloc_usable(ptr,sz,NULL)
/** @brief listpack 内存释放宏 */
#define lp_free zfree
/** @brief 获取 listpack 分配内存实际大小宏 */
#define lp_malloc_size zmalloc_usable_size

/**
 * @brief 创建新的空 listpack
 * @param capacity 初始容量（字节，用于预分配）
 * @return 新建的 listpack 指针（unsigned char*）；内存分配失败返回 NULL
 */
unsigned char *lp_new(size_t capacity);

/**
 * @brief 在 listpack 头部插入新元素
 * @param lp   原 listpack 指针（可能被 realloc 移动）
 * @param s    要插入的字节数据指针
 * @param slen 数据长度（字节）
 * @return 插入后的 listpack 指针（可能与 lp 不同）；失败返回 NULL
 */
unsigned char *lp_prepend(unsigned char *lp, unsigned char *s, uint32_t slen);

/**
 * @brief 在 listpack 尾部追加新元素
 * @param lp   原 listpack 指针（可能被 realloc 移动）
 * @param s    要追加的字节数据指针
 * @param slen 数据长度（字节）
 * @return 追加后的 listpack 指针（可能与 lp 不同）；失败返回 NULL
 */
unsigned char *lp_append(unsigned char *lp, unsigned char *s, uint32_t slen);

/**
 * @brief 获取 listpack 占用的总字节数（含头部和尾部标记）
 * @param lp listpack 指针
 * @return 占用字节数
 */
size_t lp_bytes(unsigned char *lp);

/**
 * @brief 打印 listpack 内容（调试用）
 *        将所有元素以可读格式输出到标准输出。
 * @param lp listpack 指针
 */
void lp_repr(unsigned char *lp);
#endif //LATTE_C_LISTPACK_H
