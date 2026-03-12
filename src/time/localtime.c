#include "localtime.h"
#include <limits.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stddef.h>

/**
 * @brief 判断给定年份是否为闰年（内部辅助函数）
 * 规则：能被 4 整除且不能被 100 整除，或能被 400 整除。
 * @param year 年份（如 2024）
 * @return int 闰年返回 1，平年返回 0
 */
static int _is_leap_year(time_t year) {
    if (year % 4) return 0;         /* 不能被 4 整除：平年 */
    else if (year % 100) return 1;  /* 能被 4 整除且不能被 100：闰年 */
    else if (year % 400) return 0;  /* 能被 100 但不能被 400：平年 */
    else return 1;                  /* 能被 400：闰年 */
}

/**
 * @brief 无锁版本的 UTC 时间转本地时间（线程安全）
 *
 * 不调用 localtime_r，纯算法实现：
 *   1. 减去时区偏移和夏令时修正
 *   2. 按天和秒分解，计算时分秒、星期
 *   3. 循环确定年份，再逐月确定月份和日期
 *
 * @param tmp 输出：填充后的 struct tm（tm_year 为年份-1900）
 * @param t   输入：Unix 时间戳（秒）
 * @param tz  时区偏移量（秒，东正西负）
 * @param dst 夏令时标志（1=夏令时，0=标准时）
 */
void nolocks_localtime(struct tm *tmp, time_t t, time_t tz, int dst) {
    const time_t secs_min = 60;
    const time_t secs_hour = 3600;
    const time_t secs_day = 3600*24;

    t -= tz;                            /* 调整时区偏移 */
    t += 3600*dst;                      /* 调整夏令时 */
    time_t days = t / secs_day;         /* 自 epoch 起经过的天数 */
    time_t seconds = t % secs_day;      /* 当天剩余秒数 */

    tmp->tm_isdst = dst;
    tmp->tm_hour = seconds / secs_hour;
    tmp->tm_min = (seconds % secs_hour) / secs_min;
    tmp->tm_sec = (seconds % secs_hour) % secs_min;

    /* 1970-01-01 是星期四（tm_wday=4），加 4 后取模 7 得星期 */
    tmp->tm_wday = (days+4)%7;

    /* 从 1970 年开始逐年减去天数，确定当前年份 */
    tmp->tm_year = 1970;
    while(1) {
        time_t days_this_year = 365 + _is_leap_year(tmp->tm_year);
        if (days_this_year > days) break;
        days -= days_this_year;
        tmp->tm_year++;
    }
    tmp->tm_yday = days; /* 当年中的第几天（0-based） */

    /* 逐月减去天数，确定月份和日期；闰年 2 月多 1 天 */
    int mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    mdays[1] += _is_leap_year(tmp->tm_year);

    tmp->tm_mon = 0;
    while(days >= mdays[tmp->tm_mon]) {
        days -= mdays[tmp->tm_mon];
        tmp->tm_mon++;
    }

    tmp->tm_mday = days+1;   /* days 是 0-based，加 1 得日期 */
    tmp->tm_year -= 1900;    /* tm_year 存储年份-1900 */
}

/**
 * @brief 获取本地时区相对 UTC 的偏移量（秒）
 * Linux/Solaris 直接读取 timezone 全局变量；其他系统通过 gettimeofday 获取。
 * @return long 时区偏移秒数（东区为正，西区为负）
 */
long get_time_zone(void) {
#if defined(__linux__) || defined(__sun)
    return timezone;
#else
    struct timeval tv;
    struct timezone tz;

    gettimeofday(&tv, &tz);

    return tz.tz_minuteswest * 60L;
#endif
}

/**
 * @brief 获取当前时间的毫秒时间戳（ustime 除以 1000）
 * @return mstime_t 当前 Unix 时间（毫秒）
 */
mstime_t mstime(void) {
    return ustime()/1000;
}

/**
 * @brief 获取当前时间的微秒时间戳（使用 gettimeofday）
 * @return ustime_t 当前 Unix 时间（微秒）
 */
ustime_t ustime(void) {
    struct timeval tv;
    ustime_t ust;

    gettimeofday(&tv, NULL);
    ust = ((long long)tv.tv_sec)*1000000;
    ust += tv.tv_usec;
    return ust;
}

/**
 * @brief 判断给定时刻是否处于夏令时
 * ut 为 0 时使用当前时间；通过 localtime_r 填充 struct tm 并读取 tm_isdst。
 * @param ut Unix 时间戳（秒），0 表示使用当前时间
 * @return int 夏令时返回 1，否则返回 0
 */
int get_daylight_active(time_t ut) {
    if (ut == 0) {
        ut = ustime() / 1000000;
    }
    struct tm tm;
    localtime_r(&ut, &tm);
    return tm.tm_isdst;
}

/**
 * @brief 获取单调时钟当前纳秒值（CLOCK_MONOTONIC）
 * 不受系统时钟调整影响，适用于计算时间差。
 * @return unsigned long 纳秒时间戳
 */
unsigned long current_monitonic_time()
{
  struct timespec tp;
  clock_gettime(CLOCK_MONOTONIC, &tp);
  return tp.tv_sec * 1000 * 1000 * 1000UL + tp.tv_nsec;
}