#ifndef __LATTE_LOCALTIME_H
#define __LATTE_LOCALTIME_H
#include <time.h>

/** @brief 毫秒时间戳类型 */
typedef long long mstime_t;
/** @brief 微秒时间戳类型 */
typedef long long ustime_t;
/** @brief 微秒时间点类型（记录某一时刻） */
typedef long long ustime_point_t;
/** @brief 微秒时间段类型（记录时间差） */
typedef long long ustime_duration_t;

/**
 * @brief 获取本地时区偏移量（秒）
 * @return long 相对 UTC 的秒数偏移（东正西负）
 */
long get_time_zone();

/**
 * @brief 判断给定时刻是否处于夏令时
 * @param t 时间戳
 * @return int 夏令时返回 1，否则返回 0
 */
int get_daylight_active(time_t t);

/**
 * @brief 无锁版本的本地时间转换（线程安全）
 * 不依赖 localtime_r，通过手动计算时区和夏令时偏移分解时间。
 * @param tmp 输出：分解后的 struct tm
 * @param t   输入：Unix 时间戳
 * @param tz  时区偏移量（秒）
 * @param dst 夏令时标志（1=夏令时，0=标准时）
 */
void nolocks_localtime(struct tm *tmp, time_t t, time_t tz, int dst);

/**
 * @brief 获取当前时间的微秒时间戳
 * @return ustime_t 当前 Unix 时间（微秒）
 */
ustime_t ustime(void);

/**
 * @brief 获取当前时间的毫秒时间戳
 * @return mstime_t 当前 Unix 时间（毫秒）
 */
mstime_t mstime(void);

/**
 * @brief 获取单调时钟的当前值（纳秒或毫秒，取决于实现）
 * 用于计算时间差，不受系统时钟调整影响。
 * @return unsigned long 单调时钟当前值
 */
unsigned long  current_monitonic_time();


#endif
