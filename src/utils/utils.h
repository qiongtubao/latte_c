#ifndef __LATTE_UTILS_H
#define __LATTE_UTILS_H

#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "sds/sds.h"

/** @brief long double 转字符串的最大缓冲区大小（5KB） */
#define MAX_LONG_DOUBLE_CHARS 5*1024
/** @brief unsigned long long 转字符串的最大位数（ULLONG_MAX 为 20 位，加符号位共 21） */
#define MAX_ULL_CHARS 21

/* ==================== 分支预测提示宏 ==================== */
#ifdef __GNUC__
#  define likely(x)   __builtin_expect(!!(x), 1)   /**< 标记条件大概率为真 */
#  define unlikely(x) __builtin_expect(!!(x), 0)   /**< 标记条件大概率为假 */
#else
#  define likely(x)   !!(x)
#  define unlikely(x) !!(x)
#endif

/* ==================== 不可达代码标记 ==================== */
#if __GNUC__ >= 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
/** @brief 标记代码不可达（GCC 优化提示） */
#define latte_unreachable __builtin_unreachable
#else
#define latte_unreachable abort
#endif

/* ==================== 通用数值宏 ==================== */
#ifndef latte_min
/** @brief 取两值中较小者 */
#define latte_min(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef latte_max
/** @brief 取两值中较大者 */
#define latte_max(a, b) ((a) > (b) ? (a) : (b))
#endif

/* ==================== 断言工具 ==================== */

/**
 * @brief 带信息的断言（condition 为假时打印错误信息并终止）
 * @param condition 断言条件，为假时触发
 * @param message   格式化错误信息（支持 printf 风格）
 * @param ...       可变参数
 */
void latte_assert_with_info(int condition, const char *message, ...);

/* ==================== 数值与字符串转换函数 ==================== */

/**
 * @brief 将 long long 整数转换为字符串
 * @param s     输出缓冲区
 * @param len   缓冲区大小
 * @param value 要转换的整数
 * @return int 写入的字符数（不含终止符），缓冲区不足返回 0
 */
int ll2string(char *s, size_t len, long long value);

/**
 * @brief 将 long long 整数转换为 SDS 字符串
 * @param value 要转换的整数
 * @return sds_t 新建的 SDS 字符串（调用方负责释放）
 */
sds_t ll2sds(long long value);

/**
 * @brief 将字符串解析为 long long 整数
 * @param s     输入字符串
 * @param slen  字符串长度
 * @param value 输出：解析结果
 * @return int 成功返回 1，失败返回 0
 */
int string2ll(const char *s, size_t slen, long long *value);

/**
 * @brief 将 SDS 字符串解析为 long long 整数
 * @param s     输入 SDS 字符串
 * @param value 输出：解析结果
 * @return int 成功返回 1，失败返回 0
 */
int sds2ll(sds_t s, long long* value);

/**
 * @brief 将字符串解析为 unsigned long long 整数
 * @param s     输入字符串（以 '\0' 结尾）
 * @param value 输出：解析结果
 * @return int 成功返回 1，失败返回 0
 */
int string2ull(const char *s, unsigned long long *value);

/**
 * @brief 将 unsigned long long 整数转换为字符串
 * @param s     输出缓冲区
 * @param len   缓冲区大小
 * @param value 要转换的整数
 * @return int 写入的字符数
 */
int ull2string(char *s, size_t len, unsigned long long value);

/**
 * @brief 将 unsigned long long 整数转换为 SDS 字符串
 * @param value 要转换的整数
 * @return sds_t 新建的 SDS 字符串
 */
sds_t ull2sds(unsigned long long value);

/**
 * @brief 将字符串解析为 long double 浮点数
 * @param s    输入字符串
 * @param slen 字符串长度
 * @param dp   输出：解析结果
 * @return int 成功返回 1，失败返回 0
 */
int string2ld(const char *s, size_t slen, long double *dp);

/**
 * @brief 将 SDS 字符串解析为 long double 浮点数
 * @param s  输入 SDS 字符串
 * @param dp 输出：解析结果
 * @return int 成功返回 1，失败返回 0
 */
int sds2ld(sds_t s, long double* dp);

/**
 * @brief long double 转字符串格式枚举
 */
typedef enum {
    LD_STR_AUTO,    /**< 自动格式：%.17Lg */
    LD_STR_HUMAN,   /**< 人类可读：%.17Lf（去除末尾零） */
    LD_STR_HEX      /**< 十六进制：%La */
} ld2string_mode;

/**
 * @brief 将 long double 转换为字符串
 * @param buf   输出缓冲区
 * @param len   缓冲区大小
 * @param value 要转换的值
 * @param mode  转换格式（LD_STR_AUTO/HUMAN/HEX）
 * @return int 写入的字符数，失败返回 -1
 */
int ld2string(char *buf, size_t len, long double value, ld2string_mode mode);

/**
 * @brief 将 long double 转换为 SDS 字符串
 * @param value 要转换的值
 * @param mode  转换格式
 * @return sds_t 新建的 SDS 字符串
 */
sds_t ld2sds(long double value, ld2string_mode mode);

/**
 * @brief 将字符串解析为 double 浮点数
 * @param s    输入字符串
 * @param slen 字符串长度
 * @param dp   输出：解析结果
 * @return int 成功返回 1，失败返回 0
 */
int string2d(const char *s, size_t slen, double *dp);

/**
 * @brief 将 SDS 字符串解析为 double 浮点数
 * @param value 输入 SDS 字符串
 * @param dp    输出：解析结果
 * @return int 成功返回 1，失败返回 0
 */
int sds2d(sds_t value, double* dp);

/**
 * @brief 将 double 转换为字符串
 * @param buf   输出缓冲区
 * @param len   缓冲区大小
 * @param value 要转换的值
 * @return int 写入的字符数
 */
int d2string(char *buf, size_t len, double value);

/**
 * @brief 将 double 转换为 SDS 字符串
 * @param value 要转换的值
 * @return sds_t 新建的 SDS 字符串
 */
sds_t d2sds(double value);

/* ==================== 数位计算工具 ==================== */

/**
 * @brief 计算无符号 64 位整数的十进制位数
 * @param v 输入值
 * @return uint32_t 十进制位数（v=0 时返回 1）
 */
uint32_t digits10(uint64_t v);

/**
 * @brief 计算有符号 64 位整数的十进制位数（含负号）
 * @param v 输入值
 * @return uint32_t 十进制位数（负数包含负号位）
 */
uint32_t sdigits10(int64_t v);
#endif
