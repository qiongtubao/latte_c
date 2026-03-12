
#ifndef __LATTE_INT_SET_H
#define __LATTE_INT_SET_H
#include <stdint.h>
#include <string.h>

/**
 * @brief 整数集合的编码方式宏定义
 * 注意这些编码方式是有序的：
 * INTSET_ENC_INT16 < INTSET_ENC_INT32 < INTSET_ENC_INT64.
 */
/* Note that these encodings are ordered, so:
 * INTSET_ENC_INT16 < INTSET_ENC_INT32 < INTSET_ENC_INT64. */
#define INTSET_ENC_INT16 (sizeof(int16_t))
#define INTSET_ENC_INT32 (sizeof(int32_t))
#define INTSET_ENC_INT64 (sizeof(int64_t))

/**
 * @brief Redis风格的紧凑型整数集合结构体
 * 元素以从小到大的顺序紧凑地存储在contents数组中
 * 所有整数均使用相同的编码方式，在插入更大的整数时会自动升级编码
 */
typedef struct int_set_t {
    uint32_t encoding;      /**< 编码方式，表示当前集合使用的元素大小 (2, 4 或 8 字节) */
    uint32_t length;        /**< 集合中元素的数量 */
    int8_t contents[];      /**< 柔性数组，连续存储所有整数元素的字节序列 */
} int_set_t;

/**
 * @brief 创建一个新的空整数集合
 * @return int_set_t* 返回创建的整数集合
 */
int_set_t* int_set_new();

/**
 * @brief 向整数集合中添加一个整数
 * @param is 整数集合指针
 * @param value 要添加的整数值
 * @param success 输出参数，指向一个用于接收是否添加成功(1表示新添加，0表示已存在)的变量
 * @return int_set_t* 返回可能被重新分配内存后的整数集合指针
 */
int_set_t* int_set_add(int_set_t* is, int64_t value, uint8_t *success);

/**
 * @brief 从整数集合中移除一个整数
 * @param is 整数集合指针
 * @param value 要移除的整数值
 * @param success 输出参数，接收是否移除成功的结果
 * @return int_set_t* 返回可能被重新分配内存后的整数集合指针
 */
int_set_t *int_set_remove(int_set_t *is, int64_t value, int *success);

/**
 * @brief 检查整数集合中是否包含特定的整数
 * @param is 整数集合指针
 * @param value 要查找的整数
 * @return uint8_t 1表示找到，0表示未找到
 */
uint8_t int_set_find(int_set_t *is, int64_t value);

/**
 * @brief 随机返回整数集合中的一个整数
 * @param is 整数集合指针
 * @return int64_t 随机取出的整数值
 */
int64_t int_set_random(int_set_t *is);

/**
 * @brief 获取整数集合中指定索引位置的元素
 * @param is 整数集合指针
 * @param pos 要获取的索引位置 (0-based)
 * @param value 输出参数，保存获取到的值
 * @return uint8_t 1表示获取成功，0表示越界
 */
uint8_t int_set_get(int_set_t *is, uint32_t pos, int64_t *value);

/**
 * @brief 获取整数集合的元素数量
 * @param is 整数集合指针
 * @return uint32_t 元素数量
 */
uint32_t int_set_len(const int_set_t *is);

/**
 * @brief 获取整个整数集合结构体所占用的内存字节数
 * @param is 整数集合指针
 * @return size_t 字节数
 */
size_t int_set_blob_len(int_set_t *is);

/**
 * @brief 验证整数集合的完整性（用于网络传输或序列化后的检查）
 * @param is 指向整数集合数据的指针
 * @param size 数据的总字节大小
 * @param deep 是否进行深度检查（检查元素是否有序且唯一）
 * @return int 1表示有效，0表示损坏
 */
int int_set_validate_integrity(const unsigned char *is, size_t size, int deep);

/**
 * @brief 返回能够容纳指定值所需的最小编码方式
 * @param v 要评估的整数值
 * @return uint8_t 编码方式 (INTSET_ENC_INT16/32/64)
 */
uint8_t _int_set_value_encoding(int64_t v);

/**
 * @brief 在整数集合中搜索指定值的位置（如果不存在则返回应该插入的位置）
 * @param is 整数集合指针
 * @param value 要搜索的整数值
 * @param pos 输出参数，如果找到则为该元素索引，未找到则为它应插入的位置
 * @return uint8_t 1表示找到，0表示未找到
 */
uint8_t int_set_search(int_set_t *is, int64_t value, uint32_t *pos);
#endif