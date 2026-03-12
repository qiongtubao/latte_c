/**
 * @file bitmap.c
 * @brief 位图数据结构实现
 *        提供高效的位操作功能，支持位的设置、清除、查找等操作
 *        基于SDS字符串实现，支持动态大小调整和迭代器遍历
 */

#include "bitmap.h"

/**
 * @brief 创建新的位图
 *        分配指定长度的位图并清零所有位
 * @param len 位图长度（字节数）
 * @return 新创建的位图；失败返回NULL
 */
bitmap_t bitmap_new(int len) {
    sds_t map = sds_new_len("", len);
    if (map == NULL) return NULL;
    bitmap_clear_all(map);  // 清零所有位
    return map;
}

/**
 * @brief 清零位图中的所有位
 * @param map 要清零的位图
 */
void bitmap_clear_all(bitmap_t map) {
    int size = bit_size(map);
    for (int iter = 0;  iter < size; iter++) {
        map[iter] = 0;  // 将每个字节设为0
    }
}

/**
 * @brief 获取指定位置的位值
 * @param map   位图
 * @param index 位索引
 * @return true 位已设置；false 位未设置
 */
bool bitmap_get(bitmap_t map ,int index) {
    char bits = map[index/8];  // 定位到对应字节
    return (bits & (1 << (index %8))) != 0;  // 检查对应位是否设置
}

/**
 * @brief 设置指定位置的位
 * @param map   位图
 * @param index 位索引
 */
void bitmap_set(bitmap_t map, int index) {
    char* bits = &map[index/8];  // 定位到对应字节
    *bits |= (1 << (index %8));  // 设置对应位
}

/**
 * @brief 清除指定位置的位
 * @param map   位图
 * @param index 位索引
 */
void bitmap_clear(bitmap_t map, int index) {
    char* bits = &map[index/8];   // 定位到对应字节
    *bits &= ~(1 << (index % 8)); // 清除对应位
}

/**
 * @brief 计算存储指定位数所需的字节数
 * @param size 位数
 * @return 所需字节数
 */
int bytes(int size) { return size % 8 == 0 ? size / 8 : size / 8 + 1; }

/**
 * @brief 在字节中查找第一个为0的位
 * @param byte  要搜索的字节
 * @param start 开始搜索的位位置
 * @return 第一个0位的索引；-1表示未找到
 */
int find_char_first_zero(char byte, int start)
{
  for (int i = start; i < 8; i++) {
    if ((byte & (1 << i)) == 0) {  // 检查第i位是否为0
      return i;
    }
  }
  return -1;
}

/**
 * @brief 在字节中查找第一个为1的位
 * @param byte  要搜索的字节
 * @param start 开始搜索的位位置
 * @return 第一个1位的索引；-1表示未找到
 */
int find_char_first_setted(char byte, int start)
{
  for (int i = start; i < 8; i++) {
    if ((byte & (1 << i)) != 0) {  // 检查第i位是否为1
      return i;
    }
  }
  return -1;
}

/**
 * @brief 查找下一个未设置的位
 * @param map   位图
 * @param start 开始搜索的位置
 * @return 下一个未设置位的索引；-1表示未找到
 */
int bitmap_next_unsetted(bitmap_t map, int start) {
    int ret = -1;
    int start_in_byte = start % 8;  // 在字节中的起始位置
    int size = bit_size(map);
    for (int iter = start / 8 , end = bytes(size) ; iter < end; iter++) {
        char byte = map[iter];
        if (byte != -1) {  // 如果字节不是全1，则可能有0位
            int index_in_byte = find_char_first_zero(byte, start_in_byte);
            if (index_in_byte >= 0) {
                ret = iter * 8 + index_in_byte;
                break;
            }
        }
        start_in_byte = 0;  // 后续字节从位置0开始搜索
    }
    if (ret >= size) {  // 确保不超出位图范围
        ret = -1;
    }
    return ret;
}

/**
 * @brief 查找下一个已设置的位
 * @param map   位图
 * @param start 开始搜索的位置
 * @return 下一个已设置位的索引；-1表示未找到
 */
int bitmap_next_setted(bitmap_t map, int start) {
    int ret = -1;
    int start_in_byte = start % 8;  // 在字节中的起始位置
    int size = bit_size(map);
    for (int iter = start / 8, end = bytes(size); iter < end; iter++) {
        char byte = map[iter];
        if (byte != 0x00) {  // 如果字节不是全0，则可能有1位
            int index_in_byte = find_char_first_setted(byte, start_in_byte);
            if (index_in_byte >= 0) {
                ret = iter * 8 + index_in_byte;
                break;
            }
        }
        start_in_byte = 0;  // 后续字节从位置0开始搜索
    }
    if (ret >= size) {  // 确保不超出位图范围
        ret = -1;
    }
    return ret;
}

//iterator

/**
 * @brief 位图迭代器数据结构
 */
typedef struct bitmap_iterator_data_t {
    bitmap_t bitmap;  /**< 位图引用 */
    int start;        /**< 当前位置 */
    int next;         /**< 下一个位置 */
} bitmap_iterator_data_t;

#define UNFIND -1      // 未找到标记
#define WILLFIND -2    // 待查找标记

/**
 * @brief 已设置位迭代器检查是否有下一个元素
 * @param iterator 迭代器实例
 * @return true 有下一个元素；false 没有
 */
bool setted_iterator_has_next(latte_iterator_t* iterator) {
    bitmap_iterator_data_t* data = iterator->data;
    data->next = bitmap_next_setted(data->bitmap, data->start + 1);
    return data->next != UNFIND;
}

/**
 * @brief 位图迭代器获取下一个元素
 * @param iterator 迭代器实例
 * @return 下一个位置索引（void*类型）；NULL表示结束
 */
void* bitmap_iterator_next(latte_iterator_t* iterator) {
    bitmap_iterator_data_t* data = iterator->data;
    if (data->next == UNFIND) return NULL;
    data->start = data->next;
    return (void*)(long)data->next;  // 返回位置索引
}

/**
 * @brief 删除位图迭代器
 * @param iterator 要删除的迭代器
 */
void bitmap_iterator_delete(latte_iterator_t* iterator) {
    zfree(iterator->data);
    zfree(iterator);
}

/**
 * @brief 已设置位迭代器函数表
 */
latte_iterator_func latte_iterator_setted_func = {
    .has_next  = setted_iterator_has_next,
    .next = bitmap_iterator_next,
    .release = bitmap_iterator_delete
};

/**
 * @brief 创建位图迭代器数据
 * @param map   位图
 * @param start 开始位置
 * @return 新创建的迭代器数据
 */
bitmap_iterator_data_t* bitmap_iterator_data_new(bitmap_t map, int start) {
    bitmap_iterator_data_t* data = zmalloc(sizeof(bitmap_iterator_data_t));
    data->bitmap = map;
    data->start = start-1;  // 从start-1开始，便于has_next查找start位置
    data->next = WILLFIND;
    return data;
}

/**
 * @brief 获取已设置位的迭代器
 *        用于遍历位图中所有设置为1的位
 * @param map   位图
 * @param start 开始位置
 * @return 已设置位迭代器
 */
latte_iterator_t* bitmap_get_setted_iterator(bitmap_t map, int start) {
    latte_iterator_t* iterator = latte_iterator_new(&latte_iterator_setted_func,
    bitmap_iterator_data_new(map, start));
    return iterator;
}

/**
 * @brief 未设置位迭代器检查是否有下一个元素
 * @param iterator 迭代器实例
 * @return true 有下一个元素；false 没有
 */
bool unsetted_iterator_has_next(latte_iterator_t* iterator) {
    bitmap_iterator_data_t* data = iterator->data;
    data->next = bitmap_next_unsetted(data->bitmap, data->start + 1);
    return data->next != UNFIND;
}

/**
 * @brief 未设置位迭代器函数表
 */
latte_iterator_func latte_iterator_unsetted_func = {
    .has_next  = unsetted_iterator_has_next,
    .next = bitmap_iterator_next,
    .release = bitmap_iterator_delete
};

/**
 * @brief 获取未设置位的迭代器
 *        用于遍历位图中所有设置为0的位
 * @param map   位图
 * @param start 开始位置
 * @return 未设置位迭代器
 */
latte_iterator_t* bitmap_get_unsetted_iterator(bitmap_t map, int start) {
    latte_iterator_t* iterator = latte_iterator_new(&latte_iterator_unsetted_func,
    bitmap_iterator_data_new(map, start));
    return iterator;
}