/**
 * @file bitmap.h
 * @brief 位图模块接口
 *        基于 sds 字符串实现的紧凑位图，支持位的设置、清除、查询及迭代遍历。
 *        使用 sds_len 获取位图的位数（byte 长度 × 8 / 实际以字节为单位）。
 */
#ifndef __LATTE_BITMAP_H
#define __LATTE_BITMAP_H

#include "sds/sds.h"
#include <stdbool.h>
#include "iterator/iterator.h"

/** @brief 位图类型别名（底层为 sds 字符串） */
#define bitmap_t sds
/** @brief 获取位图字节长度（即可表示的 bit 数 / 8） */
#define bit_size sds_len

/**
 * @brief 创建指定位数的位图（所有位初始化为 0）
 * @param size 位图位数（以 bit 为单位）
 * @return 新建的位图；内存不足时返回 NULL
 */
bitmap_t bitmap_new(int size);

/**
 * @brief 将位图所有位清零
 * @param map 目标位图
 */
void bitmap_clear_all(bitmap_t map);

/**
 * @brief 获取指定位的值
 * @param map   目标位图
 * @param index 位索引（从 0 开始）
 * @return 该位为 1 返回 true；为 0 返回 false
 */
bool bitmap_get(bitmap_t map, int index);

/**
 * @brief 将指定位设置为 1
 * @param map   目标位图
 * @param index 位索引（从 0 开始）
 */
void bitmap_set(bitmap_t map, int index);

/**
 * @brief 将指定位清零（设为 0）
 * @param map   目标位图
 * @param index 位索引（从 0 开始）
 */
void bitmap_clear(bitmap_t map, int index);

/**
 * @brief 从 start 位置起查找下一个值为 0 的位
 * @param map   目标位图
 * @param start 起始位索引（含）
 * @return 找到的位索引；未找到返回 -1
 */
int bitmap_next_unsetted(bitmap_t map, int start);

/**
 * @brief 从 start 位置起查找下一个值为 1 的位
 * @param map   目标位图
 * @param start 起始位索引（含）
 * @return 找到的位索引；未找到返回 -1
 */
int bitmap_next_setted(bitmap_t map, int start);

/**
 * @brief 获取从 start 开始遍历所有已置位（值为 1）的位的迭代器
 * @param map   目标位图
 * @param start 起始位索引（含）
 * @return 新建的迭代器指针；调用方负责释放
 */
latte_iterator_t* bitmap_get_setted_iterator(bitmap_t map, int start);

/**
 * @brief 获取从 start 开始遍历所有未置位（值为 0）的位的迭代器
 * @param map   目标位图
 * @param start 起始位索引（含）
 * @return 新建的迭代器指针；调用方负责释放
 */
latte_iterator_t* bitmap_get_unsetted_iterator(bitmap_t map, int start);

#endif
