#ifndef __LATTE_VALUE_H
#define __LATTE_VALUE_H

#include "sds/sds.h"
#include "vector/vector.h"
#include <stdbool.h>
#include "dict/dict.h"

/**
 * @brief 通用值类型枚举
 * 表示 value_t 联合体中实际存储的数据类型。
 */
typedef enum value_type_enum {
    VALUE_UNDEFINED,        /**< 未定义/空值 */
    VALUE_CONSTANT_CHAR,    /**< 常量字符串（char* 指针，不拥有内存） */
    VALUE_SDS,              /**< 动态字符串（SDS，拥有内存） */
    VALUE_INT,              /**< 64 位有符号整数 */
    VALUE_UINT,             /**< 64 位无符号整数 */
    VALUE_DOUBLE,           /**< 长双精度浮点数 */
    VALUE_BOOLEAN,          /**< 布尔值 */
    VALUE_ARRAY,            /**< 数组（vector_t* 指针） */
    VALUE_MAP,              /**< 哈希表（dict_t* 指针） */
} value_type_enum;

/**
 * @brief 通用值结构体
 * 使用类型标签（type）+ 联合体实现多类型值的统一表示。
 */
typedef struct value_t {
    value_type_enum type;   /**< 当前存储的值类型 */
    union {
        sds_t sds_value;            /**< SDS 字符串值 */
        int64_t i64_value;          /**< 有符号 64 位整数值 */
        uint64_t  u64_value;        /**< 无符号 64 位整数值 */
        long double ld_value;       /**< 长双精度浮点值 */
        bool bool_value;            /**< 布尔值 */
        vector_t* array_value;      /**< 数组指针值 */
        dict_t* map_value;          /**< 哈希表指针值 */
    } value;                        /**< 实际数据联合体 */
} value_t;

/* ==================== 生命周期 ==================== */

/**
 * @brief 在堆上分配一个未定义类型的 value_t
 * @return value_t* 新建值对象指针
 */
value_t* value_new();

/**
 * @brief 释放 value_t 及其持有的资源（SDS/array/map 等）
 * @param v 目标值指针
 */
void value_delete(value_t* v);

/**
 * @brief 清理 value_t 内部资源并将类型重置为 VALUE_UNDEFINED
 * @param v 目标值指针
 */
void value_clean(value_t* v);

/* ==================== 读取函数（不检查类型） ==================== */

/** @brief 读取 SDS 字符串值 */
sds_t value_get_sds(value_t* v);
/** @brief 读取有符号 64 位整数值 */
int64_t value_get_int64(value_t* v);
/** @brief 读取无符号 64 位整数值 */
uint64_t value_get_uint64(value_t* v);
/** @brief 读取长双精度浮点值 */
long double value_get_long_double(value_t* v);
/** @brief 读取布尔值 */
bool value_get_bool(value_t* v);
/** @brief 读取数组指针 */
vector_t* value_get_array(value_t* v);
/** @brief 读取哈希表指针 */
dict_t* value_get_map(value_t* v);
/** @brief 读取常量字符串指针 */
char* value_get_constant_char(value_t* v);
/** @brief 以二进制方式读取值（返回 SDS 形式） */
sds_t value_get_binary(value_t* v);

/* ==================== 写入函数 ==================== */

/** @brief 设置 SDS 字符串值（类型置为 VALUE_SDS） */
void value_set_sds(value_t* v, sds_t s);
/** @brief 设置有符号 64 位整数值 */
void value_set_int64(value_t* v, int64_t l);
/** @brief 设置无符号 64 位整数值 */
void value_set_uint64(value_t* v, uint64_t ull);
/** @brief 设置长双精度浮点值 */
void value_set_long_double(value_t* v, long double d);
/** @brief 设置布尔值 */
void value_set_bool(value_t* v, bool b);
/** @brief 设置数组指针 */
void value_set_array(value_t* v, vector_t* ve);
/** @brief 设置哈希表指针 */
void value_set_map(value_t* v, dict_t* d);
/** @brief 设置常量字符串指针（不拷贝，不拥有内存） */
void value_set_constant_char(value_t* v, char* c);
/**
 * @brief 将二进制数据设置为指定类型
 * @param v    目标值指针
 * @param type 目标类型
 * @param s    二进制数据指针
 * @param len  数据长度
 * @return int 成功返回 1，失败返回 0
 */
int value_set_binary(value_t* v, value_type_enum type, char* s, int len);

/* ==================== 类型检查宏 ==================== */

#define value_is_int64(v)         (v != NULL && v->type == VALUE_INT)         /**< 是否为有符号整数 */
#define value_is_uint64(v)        (v != NULL && v->type == VALUE_UINT)        /**< 是否为无符号整数 */
#define value_is_long_double(v)   (v!= NULL && v->type == VALUE_DOUBLE)       /**< 是否为浮点数 */
#define value_is_bool(v)          (v != NULL && v->type == VALUE_BOOLEAN)     /**< 是否为布尔值 */
#define value_is_sds(v)           (v != NULL && v->type == VALUE_SDS)         /**< 是否为 SDS 字符串 */
#define value_is_array(v)         (v != NULL && v->type == VALUE_ARRAY)       /**< 是否为数组 */
#define value_is_map(v)           (v != NULL &&  v->type == VALUE_MAP)        /**< 是否为哈希表 */
#define value_is_constant_char(v) (v != NULL && v->type == VALUE_CONSTANT_CHAR) /**< 是否为常量字符串 */
#define value_is_null(v)          (v == NULL || v->type == VALUE_UNDEFINED)   /**< 是否为空/未定义 */

/* ==================== 比较函数 ==================== */

/**
 * @brief 比较两个 value_t（void* 版本，用于 cmp_func 回调）
 * @param a value_t* 指针（以 void* 传入）
 * @param b value_t* 指针（以 void* 传入）
 * @return int 负数/0/正数 对应 a<b/a==b/a>b
 */
int value_cmp(void* a, void* b);

/**
 * @brief 比较两个 value_t（强类型版本）
 * @param a 第一个值指针
 * @param b 第二个值指针
 * @return int 负数/0/正数 对应 a<b/a==b/a>b
 */
int private_value_cmp(value_t* a, value_t* b);

#endif
