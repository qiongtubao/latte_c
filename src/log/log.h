#ifndef __LATTE_LOG_H
#define __LATTE_LOG_H

#include <stdio.h>
#include <sds/sds.h>
#include <dict/dict.h>

#include <stdarg.h>
#include <stdbool.h>
#include <time.h>
#include "zmalloc/zmalloc.h"
#include <sys/time.h>

/** @brief 日志模块版本号 */
#define LOG_VERSION "0.0.1"

/**
 * @brief 日志级别枚举，从低到高依次为 TRACE/DEBUG/INFO/WARN/ERROR/FATAL
 */
typedef enum log_level_enum {
  LOG_TRACE, /**< 最详细的跟踪级别 */
  LOG_DEBUG, /**< 调试信息 */
  LOG_INFO,  /**< 一般信息 */
  LOG_WARN,  /**< 警告信息 */
  LOG_ERROR, /**< 错误信息 */
  LOG_FATAL  /**< 致命错误 */
} log_level_enum;

/**
 * @brief 日志事件结构体，封装一次日志调用的所有上下文信息
 */
typedef struct log_event_t {
  va_list ap;          /**< 可变参数列表，用于格式化日志内容 */
  const char *fmt;     /**< 格式化字符串 */
  const char *file;    /**< 产生日志的源文件名 */
  struct timeval time; /**< 日志产生时的时间（精确到微秒） */
  void *udata;         /**< 回调函数的用户数据（如 FILE* 或文件路径） */
  int line;            /**< 产生日志的行号 */
  int level;           /**< 日志级别 */
  const char* func;    /**< 产生日志的函数名 */
  int pid;             /**< 产生日志的进程 ID */
} log_event_t;

/** @brief 日志回调函数类型，接收一个日志事件并进行输出 */
typedef void (*log_func)(log_event_t *ev);
/** @brief 日志锁函数类型，lock=true 加锁，lock=false 解锁 */
typedef void (*log_lock_func)(bool lock, void *udata);

/** @brief 每个 logger 最多支持的回调数量 */
#define MAX_CALLBACKS 32

/**
 * @brief 单个日志回调注册项，包含回调函数、用户数据和最低触发级别
 */
typedef struct {
  log_func fn;  /**< 回调函数指针 */
  void *udata;  /**< 传递给回调的用户数据（如 FILE* 或文件路径） */
  int level;    /**< 该回调触发的最低日志级别 */
} log_callback_t;

/**
 * @brief 单个 logger 实例，包含锁、安静模式和最多 MAX_CALLBACKS 个回调
 */
struct logger_t {
  void *udata;                          /**< 锁函数的用户数据 */
  log_lock_func lock;                   /**< 自定义加锁/解锁函数，NULL 表示不加锁 */
  int level;                            /**< 该 logger 的最低输出级别 */
  bool quiet;                           /**< 安静模式：true 时不输出到 stderr */
  log_callback_t callbacks[MAX_CALLBACKS]; /**< 已注册的回调数组 */
};

/**
 * @brief 全局 logger 工厂，管理所有按 tag 注册的 logger 实例
 * @note timezone 和 daylight_active 暂放此处，后续可能移至 time 模块
 */
static struct logger_factory_t {
    dict_t* loggers;     /**< tag -> logger_t* 的字典 */
    long timezone;       /**< 本地时区偏移（秒），用于日志时间格式化 */
    int daylight_active; /**< 夏令时是否有效 */
} global_logger_factory;

/**
 * @brief 初始化日志模块，创建全局 logger 工厂字典并获取时区信息
 */
void log_module_init();

/**
 * @brief 内部日志记录函数，通常通过宏调用
 * @param tag      logger 标签
 * @param level    日志级别
 * @param file     源文件名（__FILE__）
 * @param function 函数名（__FUNCTION__）
 * @param line     行号（__LINE__）
 * @param fmt      格式化字符串
 * @param ...      格式化参数
 */
void log_log(char* tag, int level, char *file, const char* function, int line, char *fmt, ...);

/** @brief 输出 TRACE 级别日志的宏，自动填充文件/函数/行号 */
#define log_trace(tag, ...) log_log(tag, LOG_TRACE, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
/** @brief 输出 DEBUG 级别日志的宏 */
#define log_debug(tag, ...) log_log(tag, LOG_DEBUG, __FILE__, __FUNCTION__,  __LINE__, __VA_ARGS__)
/** @brief 输出 INFO 级别日志的宏 */
#define log_info(tag, ...)  log_log(tag, LOG_INFO,  __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
/** @brief 输出 WARN 级别日志的宏 */
#define log_warn(tag, ...)  log_log(tag, LOG_WARN,  __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
/** @brief 输出 ERROR 级别日志的宏 */
#define log_error(tag, ...) log_log(tag, LOG_ERROR, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
/** @brief 输出 FATAL 级别日志的宏 */
#define log_fatal(tag, ...) log_log(tag, LOG_FATAL, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

/**
 * @brief 获取日志级别对应的字符串名称（如 "DEBUG"、"INFO"）
 * @param level 日志级别枚举值
 * @return const char* 级别名称字符串
 */
const char* log_level_string(int level);

/**
 * @brief 为指定 tag 的 logger 设置自定义加锁函数
 * @param tag   logger 标签
 * @param fn    加锁函数指针
 * @param udata 传递给加锁函数的用户数据
 */
void log_set_lock(char* tag, log_lock_func fn, void *udata);

/**
 * @brief 设置指定 tag 的 logger 的最低输出级别
 * @param tag   logger 标签
 * @param level 新的日志级别
 */
void log_set_level(char* tag, int level);

/**
 * @brief 设置指定 tag 的 logger 的安静模式
 * @param tag    logger 标签
 * @param enable true 开启安静模式，false 关闭
 */
void log_set_quiet(char* tag, bool enable);

/**
 * @brief 为指定 tag 的 logger 注册一个自定义回调
 * @param tag   logger 标签
 * @param fn    回调函数
 * @param udata 传递给回调的用户数据
 * @param level 触发该回调的最低日志级别
 * @return int 成功返回 1，回调槽已满返回 0
 */
int log_add_callback(char* tag, log_func fn, void *udata, int level);

/**
 * @brief 为指定 tag 的 logger 注册文件输出回调
 * @param tag   logger 标签
 * @param fp    日志文件路径
 * @param level 触发文件输出的最低日志级别
 * @return int 成功返回 1，失败返回 0
 */
int log_add_file(char* tag, char *fp, int level);

/**
 * @brief 为指定 tag 的 logger 注册标准输出回调
 * @param tag   logger 标签
 * @param level 触发标准输出的最低日志级别
 * @return int 成功返回 1，失败返回 0
 */
int log_add_stdout(char* tag, int level);

/** @brief latte 内部库使用的默认日志 tag */
#define LATTE_LIB "latte_lib"
/** @brief latte 内部库日志宏，自动填充文件/函数/行号 */
#define LATTE_LIB_LOG(level, ...) log_log(LATTE_LIB, level, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

/** @brief 调用栈回溯缓冲区大小（字节） */
#define BT_BUFFER_SIZE 8192

/**
 * @brief 获取当前调用栈的符号化回溯字符串
 * @param backtrace_buffer 调用方提供的缓冲区，大小至少为 BT_BUFFER_SIZE
 * @return char* 返回填充了回溯信息的 backtrace_buffer 指针
 */
char *lbt(char* backtrace_buffer);
#endif
