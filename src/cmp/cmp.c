/**
 * @file cmp.c
 * @brief 通用比较函数实现
 *        提供各种数据类型的比较函数，用于排序、查找等操作
 *        支持字符串、整数、浮点数等常用数据类型的比较
 */

#include "cmp.h"
#include <string.h>
#include <strings.h>

/**
 * @brief 字符串比较函数（void*版本）
 *        用于需要void*参数的通用比较场景
 * @param a 第一个字符串指针
 * @param b 第二个字符串指针
 * @return <0 a<b；0 a==b；>0 a>b
 */
int str_cmp(void* a, void* b) {
    return private_str_cmp((char*)a, (char*)b);
}

/**
 * @brief 字符串比较函数（char*版本）
 *        直接比较两个字符串的字典序
 * @param a 第一个字符串
 * @param b 第二个字符串
 * @return <0 a<b；0 a==b；>0 a>b
 */
int private_str_cmp(char* a, char* b) {
    return strcmp(a, b);  // 使用标准库strcmp函数
}

/**
 * @brief 64位有符号整数比较函数（void*版本）
 *        用于需要void*参数的通用比较场景
 * @param a 第一个整数（以void*传递）
 * @param b 第二个整数（以void*传递）
 * @return <0 a<b；0 a==b；>0 a>b
 */
int int64_cmp(void* a, void* b) {
    return private_int64_cmp((int64_t)a, (int64_t)b);
}

/**
 * @brief 64位有符号整数比较函数（int64_t版本）
 *        直接比较两个64位有符号整数
 * @param a 第一个整数
 * @param b 第二个整数
 * @return <0 a<b；0 a==b；>0 a>b
 */
int private_int64_cmp(int64_t a, int64_t b) {
    return a > b ? 1: (a < b? -1 : 0);
}

/**
 * @brief 64位无符号整数比较函数（void*版本）
 *        用于需要void*参数的通用比较场景
 * @param a 第一个无符号整数（以void*传递）
 * @param b 第二个无符号整数（以void*传递）
 * @return <0 a<b；0 a==b；>0 a>b
 */
int uint64_cmp(void* a, void* b) {
    return private_uint64_cmp((uint64_t)a, (uint64_t)b);
}

/**
 * @brief 64位无符号整数比较函数（uint64_t版本）
 *        直接比较两个64位无符号整数
 * @param a 第一个无符号整数
 * @param b 第二个无符号整数
 * @return <0 a<b；0 a==b；>0 a>b
 */
int private_uint64_cmp(uint64_t a, uint64_t b) {
    return a > b ? 1: (a < b? -1 : 0);
}

/**
 * @brief 长双精度浮点数比较函数（void*版本）
 *        用于需要void*参数的通用比较场景
 * @param a 第一个浮点数指针
 * @param b 第二个浮点数指针
 * @return <0 a<b；0 a==b；>0 a>b
 */
int long_double_cmp(void* a, void* b) {
    return private_long_double_cmp(*(long double*)a, *(long double*)b);
}

/**
 * @brief 长双精度浮点数比较函数（long double版本）
 *        直接比较两个长双精度浮点数
 * @param a 第一个浮点数
 * @param b 第二个浮点数
 * @return <0 a<b；0 a==b；>0 a>b
 */
int private_long_double_cmp(long double a, long double b) {
    return a > b ? 1: (a < b? -1 : 0);
}