/**
 * @file monotonic.h
 * @brief 单调时钟接口
 *        单调时钟是一个只增不减的时间源，与系统实时时间无关，
 *        仅用于相对时间计量（如计算耗时、定时器等）。
 *        根据系统架构，可能使用 CPU 指令计数器（如 x86 的 RDTSC）
 *        实现极低开销的读取，但不保证与精确时钟严格对齐（可能有轻微漂移）。
 */
#ifndef __MONOTONIC_H
#define __MONOTONIC_H

#include "fmacros.h"
#include <stdint.h>
#include <unistd.h>

/**
 * @brief 单调时间类型（微秒精度）
 *        表示相对于某任意起点的微秒计数，仅用于差值计算，不代表绝对时间。
 */
typedef uint64_t monotime;

/**
 * @brief 获取当前单调时间的函数指针（微秒）
 *        调用此函数指针获取相对于任意起点的微秒计数。
 *        由 monotonicInit() 初始化为最优实现（POSIX 或硬件计数器）。
 */
extern monotime (*getMonotonicUs)(void);

/**
 * @brief 单调时钟实现类型枚举
 */
typedef enum monotonic_clock_type {
    MONOTONIC_CLOCK_POSIX, /**< 基于 POSIX clock_gettime 的实现 */
    MONOTONIC_CLOCK_HW,    /**< 基于硬件指令计数器的实现（如 RDTSC） */
} monotonic_clock_type;

/**
 * @brief 初始化单调时钟（程序启动时调用一次）
 *        选择当前平台最优的单调时钟实现并初始化 getMonotonicUs 函数指针。
 *        可多次调用，不会有副作用。
 * @return 描述已初始化时钟类型的静态字符串（无需释放）
 */
const char *monotonicInit();

/**
 * @brief 获取当前单调时钟类型的描述字符串
 * @return 静态字符串（无需释放），如 "POSIX clock_gettime" 或 "x86 RDTSC"
 */
const char *monotonicInfoString();

/**
 * @brief 获取当前单调时钟的实现类型
 * @return MONOTONIC_CLOCK_POSIX 或 MONOTONIC_CLOCK_HW
 */
monotonic_clock_type monotonicGetType();

/**
 * @brief 记录计时起点（inline，极低开销）
 * @param start_time 输出：记录当前单调时间作为计时起点
 *
 * 使用示例：
 *   monotime myTimer;
 *   elapsedStart(&myTimer);
 *   while (elapsedMs(myTimer) < 10) {} // 循环约 10ms
 */
static inline void elapsedStart(monotime *start_time) {
    *start_time = getMonotonicUs();
}

/**
 * @brief 计算自起点以来的微秒数（inline）
 * @param start_time 由 elapsedStart 记录的起点
 * @return 距起点的微秒数
 */
static inline uint64_t elapsedUs(monotime start_time) {
    return getMonotonicUs() - start_time;
}

/**
 * @brief 计算自起点以来的毫秒数（inline）
 * @param start_time 由 elapsedStart 记录的起点
 * @return 距起点的毫秒数
 */
static inline uint64_t elapsedMs(monotime start_time) {
    return elapsedUs(start_time) / 1000;
}

#endif
