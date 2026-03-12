/**
 * @file cmp.h
 * @brief 通用比较函数接口
 *        提供字符串、int64、uint64、long double 等类型的通用比较函数，
 *        支持通过 void* 参数传递（用于排序、字典等泛型容器）。
 */
#ifndef __LATTE_CMP_H
#define __LATTE_CMP_H
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/**
 * @brief 通用比较函数类型
 *        参数均为 void* 指针，需要调用方保证实际类型正确。
 * @param a 第一个比较对象指针
 * @param b 第二个比较对象指针
 * @return a < b 返回负数；a == b 返回 0；a > b 返回正数
 */
typedef int cmp_func(void* a, void* b);

/**
 * @brief 字符串通用比较函数（void* 接口）
 *        内部将 void* 强转为 char* 后调用 private_str_cmp
 * @param a 第一个字符串指针（void*）
 * @param b 第二个字符串指针（void*）
 * @return a < b 返回负数；a == b 返回 0；a > b 返回正数
 */
int str_cmp(void* a, void* b);

/**
 * @brief 字符串底层比较函数
 * @param a 第一个字符串（char*）
 * @param b 第二个字符串（char*）
 * @return a < b 返回负数；a == b 返回 0；a > b 返回正数
 */
int private_str_cmp(char* a, char* b);

/**
 * @brief int64_t 通用比较函数（void* 接口）
 *        内部将 void* 强转为 int64_t* 后解引用比较
 * @param a 第一个 int64_t 指针（void*）
 * @param b 第二个 int64_t 指针（void*）
 * @return a < b 返回负数；a == b 返回 0；a > b 返回正数
 */
int int64_cmp(void* a, void* b);

/**
 * @brief int64_t 底层比较函数
 * @param a 第一个 int64_t 值
 * @param b 第二个 int64_t 值
 * @return a < b 返回负数；a == b 返回 0；a > b 返回正数
 */
int private_int64_cmp(int64_t a, int64_t b);

/**
 * @brief uint64_t 通用比较函数（void* 接口）
 *        内部将 void* 强转为 uint64_t* 后解引用比较
 * @param a 第一个 uint64_t 指针（void*）
 * @param b 第二个 uint64_t 指针（void*）
 * @return a < b 返回负数；a == b 返回 0；a > b 返回正数
 */
int uint64_cmp(void* a, void* b);

/**
 * @brief uint64_t 底层比较函数
 * @param a 第一个 uint64_t 值
 * @param b 第二个 uint64_t 值
 * @return a < b 返回负数；a == b 返回 0；a > b 返回正数
 */
int private_uint64_cmp(uint64_t a, uint64_t b);

/**
 * @brief long double 通用比较函数（void* 接口）
 *        由于 long double 为 16 字节，无法直接转化为 void*，
 *        此处 void* 实为 long double* 指针，内部解引用后比较。
 * @param a 第一个 long double 指针（void*，实为 long double*）
 * @param b 第二个 long double 指针（void*，实为 long double*）
 * @return a < b 返回负数；a == b 返回 0；a > b 返回正数
 */
int long_double_cmp(void* a, void* b);

/**
 * @brief long double 底层比较函数
 * @param a 第一个 long double 值
 * @param b 第二个 long double 值
 * @return a < b 返回负数；a == b 返回 0；a > b 返回正数
 */
int private_long_double_cmp(long double a, long double b);

#endif
