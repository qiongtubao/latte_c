/**
 * @file monotonic.c
 * @brief 单调时钟实现，提供高性能的单调时间获取功能
 */
#include "monotonic.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#undef NDEBUG
#include <assert.h>


/**< 单调时钟获取函数指针 */
monotime (*getMonotonicUs)(void) = NULL;

/**< 单调时钟信息字符串 */
static char monotonic_info_string[32];


/* Using the processor clock (aka TSC on x86) can provide improved performance
 * throughout Redis wherever the monotonic clock is used.  The processor clock
 * is significantly faster than calling 'clock_getting' (POSIX).  While this is
 * generally safe on modern systems, this link provides additional information
 * about use of the x86 TSC: http://oliveryang.net/2015/09/pitfalls-of-TSC-usage
 *
 * To use the processor clock, either uncomment this line, or build with
 *   CFLAGS="-DUSE_PROCESSOR_CLOCK"
#define USE_PROCESSOR_CLOCK
 */


#if defined(USE_PROCESSOR_CLOCK) && defined(__x86_64__) && defined(__linux__)
#include <regex.h>
#include <x86intrin.h>

/**< x86架构下每微秒的时钟周期数 */
static long mono_ticksPerMicrosecond = 0;

/**
 * @brief x86架构下获取单调时间（微秒）
 * @return 返回以微秒为单位的单调时间
 */
static monotime getMonotonicUs_x86() {
    return __rdtsc() / mono_ticksPerMicrosecond;
}

/**
 * @brief 初始化x86 Linux平台的单调时钟
 */
static void monotonicInit_x86linux() {
    const int bufflen = 256;
    char buf[bufflen];
    regex_t cpuGhzRegex, constTscRegex;
    const size_t nmatch = 2;
    regmatch_t pmatch[nmatch];
    int constantTsc = 0;
    int rc;

    /* 确定一微秒内的TSC时钟周期数。这是一个与处理器标准速度匹配的常量值。
     * 在现代处理器上，即使每个核心的实际时钟速度动态变化，该速度仍保持恒定。 */
    rc = regcomp(&cpuGhzRegex, "^model name\\s+:.*@ ([0-9.]+)GHz", REG_EXTENDED);
    assert(rc == 0);

    /* 同时检查constant_tsc标志是否存在。（除非这是一个非常老的CPU，否则应该存在。） */
    rc = regcomp(&constTscRegex, "^flags\\s+:.* constant_tsc", REG_EXTENDED);
    assert(rc == 0);

    FILE *cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo != NULL) {
        // 解析CPU频率
        while (fgets(buf, bufflen, cpuinfo) != NULL) {
            if (regexec(&cpuGhzRegex, buf, nmatch, pmatch, 0) == 0) {
                buf[pmatch[1].rm_eo] = '\0';
                double ghz = atof(&buf[pmatch[1].rm_so]);
                mono_ticksPerMicrosecond = (long)(ghz * 1000);
                break;
            }
        }
        // 检查constant_tsc标志
        while (fgets(buf, bufflen, cpuinfo) != NULL) {
            if (regexec(&constTscRegex, buf, nmatch, pmatch, 0) == 0) {
                constantTsc = 1;
                break;
            }
        }

        fclose(cpuinfo);
    }
    regfree(&cpuGhzRegex);
    regfree(&constTscRegex);

    if (mono_ticksPerMicrosecond == 0) {
        fprintf(stderr, "monotonic: x86 linux, unable to determine clock rate");
        return;
    }
    if (!constantTsc) {
        fprintf(stderr, "monotonic: x86 linux, 'constant_tsc' flag not present");
        return;
    }

    snprintf(monotonic_info_string, sizeof(monotonic_info_string),
            "X86 TSC @ %ld ticks/us", mono_ticksPerMicrosecond);
    getMonotonicUs = getMonotonicUs_x86;
}
#endif


#if defined(USE_PROCESSOR_CLOCK) && defined(__aarch64__)
/**< aarch64架构下每微秒的时钟周期数 */
static long mono_ticksPerMicrosecond = 0;

/**
 * @brief 读取时钟计数值
 * @return 返回虚拟时钟计数值
 */
static inline uint64_t __cntvct() {
    uint64_t virtual_timer_value;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value));
    return virtual_timer_value;
}

/**
 * @brief 读取计数器时钟频率
 * @return 返回时钟频率（Hz）
 */
static inline uint32_t cntfrq_hz() {
    uint64_t virtual_freq_value;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(virtual_freq_value));
    return (uint32_t)virtual_freq_value;    /* 高32位为保留位 */
}

/**
 * @brief aarch64架构下获取单调时间（微秒）
 * @return 返回以微秒为单位的单调时间
 */
static monotime getMonotonicUs_aarch64() {
    return __cntvct() / mono_ticksPerMicrosecond;
}

/**
 * @brief 初始化aarch64架构的单调时钟
 */
static void monotonicInit_aarch64() {
    mono_ticksPerMicrosecond = (long)cntfrq_hz() / 1000L / 1000L;
    if (mono_ticksPerMicrosecond == 0) {
        fprintf(stderr, "monotonic: aarch64, unable to determine clock rate");
        return;
    }

    snprintf(monotonic_info_string, sizeof(monotonic_info_string),
            "ARM CNTVCT @ %ld ticks/us", mono_ticksPerMicrosecond);
    getMonotonicUs = getMonotonicUs_aarch64;
}
#endif


/**
 * @brief POSIX标准方式获取单调时间（微秒）
 * @return 返回以微秒为单位的单调时间
 */
static monotime getMonotonicUs_posix() {
    /* clock_gettime()在POSIX.1b(1993)中规定。即使如此，一些系统
     * 直到很晚才支持这个功能。CLOCK_MONOTONIC在技术上是可选的，
     * 可能不被支持 - 但它似乎是通用的。
     * 如果不支持，请提供特定于系统的替代版本。 */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec) * 1000000 + ts.tv_nsec / 1000;
}

/**
 * @brief 初始化POSIX单调时钟
 */
static void monotonicInit_posix() {
    /* 确保CLOCK_MONOTONIC被支持。这应该在任何合理的当前操作系统上被支持。
     * 如果下面的断言失败，请提供适当的替代实现。 */
    struct timespec ts;
    int rc = clock_gettime(CLOCK_MONOTONIC, &ts);
    assert(rc == 0);

    snprintf(monotonic_info_string, sizeof(monotonic_info_string),
            "POSIX clock_gettime");
    getMonotonicUs = getMonotonicUs_posix;
}



/**
 * @brief 初始化单调时钟系统
 * @return 返回描述所使用时钟类型的信息字符串
 */
const char * monotonicInit() {
    #if defined(USE_PROCESSOR_CLOCK) && defined(__x86_64__) && defined(__linux__)
    if (getMonotonicUs == NULL) monotonicInit_x86linux();
    #endif

    #if defined(USE_PROCESSOR_CLOCK) && defined(__aarch64__)
    if (getMonotonicUs == NULL) monotonicInit_aarch64();
    #endif

    if (getMonotonicUs == NULL) monotonicInit_posix();

    return monotonic_info_string;
}

/**
 * @brief 获取单调时钟信息字符串
 * @return 返回描述当前使用的单调时钟类型的字符串
 */
const char *monotonicInfoString() {
    return monotonic_info_string;
}

/**
 * @brief 获取单调时钟类型
 * @return 返回单调时钟类型枚举值
 */
monotonic_clock_type monotonicGetType() {
    if (getMonotonicUs == getMonotonicUs_posix)
        return MONOTONIC_CLOCK_POSIX;
    return MONOTONIC_CLOCK_HW;
}