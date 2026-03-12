/*
 * Copyright (c) 2020 rxi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "log.h"
#include <execinfo.h>  // 提供 backtrace 和 backtrace_symbols
#include <dlfcn.h>     // 提供 dladdr
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "time/localtime.h"

/**
 * @brief 根据 tag 从全局工厂字典中查找对应的 logger 实例
 * @param tag logger 标签字符串
 * @return struct logger_t* 找到则返回 logger 指针，否则返回 NULL
 */
struct logger_t* get_logger_by_tag(char* tag) {
  dict_entry_t* entry = dict_find(global_logger_factory.loggers, tag);
  if (entry == NULL) return NULL;
  return dict_get_entry_val(entry);
}

/**
 * @brief 创建并初始化一个新的 logger 实例
 * @return struct logger_t* 返回初始化完成的 logger，所有回调槽清空，级别默认为 DEBUG
 */
struct logger_t* logger_new() {
  struct logger_t* logger = zmalloc(sizeof(struct logger_t));
  /* 初始化所有回调槽及 logger 基本属性 */
  for (int i = 0; i < MAX_CALLBACKS; i++) {
    logger->callbacks[i].fn = NULL;
    logger->callbacks[i].udata = NULL;
    logger->callbacks[i].level = LOG_DEBUG;
    logger->lock = NULL;
    logger->udata = NULL;
    logger->level = LOG_DEBUG;
    logger->quiet = false;
  }
  return logger;
}

/**
 * @brief 根据 tag 获取 logger，若不存在则新建并注册到全局工厂
 * @param tag logger 标签字符串
 * @return struct logger_t* 已存在或新建的 logger 指针
 */
struct logger_t* get_or_new_logger_by_tag(char* tag) {
    struct logger_t* logger = get_logger_by_tag(tag);
    if (logger == NULL) {
        logger = logger_new();
        /* 将新 logger 注册到全局工厂字典中 */
        dict_add(global_logger_factory.loggers, tag, logger);
    }
    return logger;
}

/**
 * @brief 释放 logger 实例占用的内存
 * @param logger 要释放的 logger 指针
 */
void logger_delete(struct logger_t* logger) {
    zfree(logger);
}

/**
 * @brief 计算 logger tag（普通字符串）的哈希值（大小写不敏感）
 * @param key tag 字符串指针
 * @return uint64_t 哈希值
 */
uint64_t logger_hash_callback(const void *key) {
    return dict_gen_case_hash_function((unsigned char*)key, strlen((char*)key));
}

/**
 * @brief 比较两个 tag 字符串是否相等
 * @param privdata 未使用
 * @param key1     第一个 tag
 * @param key2     第二个 tag
 * @return int 相等返回 1，不等返回 0
 */
int logger_compare_callback(dict_t*privdata, const void *key1, const void *key2) {
    int l1,l2;
    DICT_NOTUSED(privdata);
    l1 = strlen((char*) key1);
    l2 = strlen((char*) key2);
    if(l1 != l2) return 0;
    return memcmp(key1, key2, l1) == 0;
}

/**
 * @brief logger 字典值的析构回调，释放 logger 实例
 * @param privdata 未使用
 * @param val      指向 logger_t 的指针
 */
void logger_free_callback(dict_t* privdata, void* val) {
    DICT_NOTUSED(privdata);
    if(val != NULL) {
        logger_delete(val);
    }
}

/** @brief 全局 logger 工厂字典使用的函数表 */
static dict_func_t logger_dict_type = {
    logger_hash_callback,  /* 哈希函数 */
    NULL,                  /* key dup */
    NULL,                  /* val dup */
    logger_compare_callback, /* key compare */
    logger_free_callback,  /* val destructor：释放 logger */
    NULL,                  /* key destructor */
    NULL                   /* allow to expand */
};

/*
 * Gets the proper timezone in a more portable fashion
 * i.e timezone variables are linux specific.
 */



/**
 * @brief 初始化日志模块：创建全局 logger 字典，并获取时区/夏令时信息
 * 必须在使用任何日志函数前调用一次
 */
void log_module_init() {
  /* 创建 tag->logger_t* 的字典 */
  global_logger_factory.loggers = dict_new(&logger_dict_type);
  /* 获取本地时区偏移（秒），用于日志时间格式化 */
  global_logger_factory.timezone = get_time_zone();
  /* 获取夏令时状态 */
  global_logger_factory.daylight_active = get_daylight_active(0);
}

/** @brief 日志级别名称数组，下标对应 log_level_enum 枚举值 */
static const char *level_strings[] = {
  "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

#ifdef LOG_USE_COLOR
static const char *level_colors[] = {
  "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"
};
#endif


// static void stdout_callback(log_event_t *ev) {
//   char buf[16];
//   buf[strftime(buf, sizeof(buf), "%H:%M:%S", ev->time)] = '\0';
// #ifdef LOG_USE_COLOR
//   fprintf(
//     ev->udata, "%s %s%-5s\x1b[0m \x1b[90m%s:%s:%d:\x1b[0m ",
//     buf, level_colors[ev->level], level_strings[ev->level],
//     ev->file, ev->func, ev->line);
// #else
//   fprintf(
//     ev->udata, "%s %-5s %s:%s:%d: ",
//     buf, level_strings[ev->level], ev->file, ev->func, ev->line);
// #endif
//   vfprintf(ev->udata, ev->fmt, ev->ap);
//   fprintf(ev->udata, "\n");
//   fflush(ev->udata);
// }
/**
 * @brief 标准输出日志回调，将日志事件格式化后输出到 stdout
 * 格式：日期时间 毫秒 PID 级别 文件:函数:行号: 内容
 * @param ev 日志事件指针，ev->udata 为 FILE*（stdout）
 */
static void stdout_callback(log_event_t *ev) {
    char buf[64]; /* 足够容纳 "DD Mon YYYY HH:MM:SS.mmm" 格式 */

    struct tm tm;
    /* 使用无锁本地时间转换，避免 localtime() 的全局锁开销 */
    nolocks_localtime(&tm, ev->time.tv_sec, global_logger_factory.timezone,
                      global_logger_factory.daylight_active);

    /* 格式化日期时间部分：DD Mon YYYY HH:MM:SS. */
    int off = strftime(buf, sizeof(buf), "%d %b %Y %H:%M:%S.", &tm);
    /* 追加毫秒部分 */
    snprintf(buf + off, sizeof(buf) - off, "%03d", (int)ev->time.tv_usec / 1000);

#ifdef LOG_USE_COLOR
    /* 带 ANSI 颜色的输出格式 */
    fprintf(ev->udata, "%s %s%-5s\x1b[0m \x1b[90m%s:%s:%d:\x1b[0m ",
            buf, level_colors[ev->level], level_strings[ev->level],
            ev->file, ev->func, ev->line);
#else
    /* 无颜色格式：时间 PID 级别 文件:函数:行号 */
    fprintf(ev->udata, "%s %d %-5s %s:%s:%d: ",
            buf, ev->pid, level_strings[ev->level], ev->file, ev->func, ev->line);
#endif

    /* 输出日志内容 */
    vfprintf(ev->udata, ev->fmt, ev->ap);
    fprintf(ev->udata, "\n");
    fflush(ev->udata);
}

// static void file_callback(log_event_t *ev) {
//   char buf[64];
//   buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ev->time)] = '\0';
//   FILE* fp = fopen((sds)ev->udata, "w+");
//   fprintf(
//     fp, "%s %-5s %s:%s:%d: ",
//     buf, level_strings[ev->level], ev->file, ev->func, ev->line);
//   vfprintf(fp, ev->fmt, ev->ap);
//   fprintf(fp, "\n");
//   fflush(fp);
//   fclose(fp);
// }



/**
 * @brief 文件日志回调，将日志事件格式化后以追加方式写入文件
 * @param ev 日志事件指针，ev->udata 为文件路径字符串（SDS）
 */
static void file_callback(log_event_t *ev) {
    char buf[64];

    struct tm tm;
    /* 使用无锁本地时间转换 */
    nolocks_localtime(&tm, ev->time.tv_sec, global_logger_factory.timezone,
                      global_logger_factory.daylight_active);

    /* 格式化日期时间，追加毫秒 */
    int off = strftime(buf, sizeof(buf), "%d %b %Y %H:%M:%S.", &tm);
    snprintf(buf + off, sizeof(buf) - off, "%03d", (int)ev->time.tv_usec / 1000);

    /* 以追加模式打开日志文件（"a+" 不会清空已有内容） */
    FILE* fp = fopen((const char*)ev->udata, "a+");
    if (!fp) {
        /* 文件打开失败时静默返回，避免日志系统崩溃 */
        return;
    }

    /* 输出日志头：时间 PID 级别 文件:函数:行号 */
    fprintf(fp, "%s %d %-5s %s:%s:%d: ",
            buf, ev->pid, level_strings[ev->level], ev->file, ev->func, ev->line);

    /* 输出日志内容 */
    vfprintf(fp, ev->fmt, ev->ap);
    fprintf(fp, "\n");
    fflush(fp);
    fclose(fp);
}

/**
 * @brief 对指定 logger 加锁（如果设置了锁函数）
 * @param logger 目标 logger 指针
 */
static void lock(struct logger_t* logger)   {
  if (logger->lock) { logger->lock(true, logger->udata); }
}

/**
 * @brief 对指定 logger 解锁（如果设置了锁函数）
 * @param logger 目标 logger 指针
 */
static void unlock(struct logger_t* logger) {
  if (logger->lock) { logger->lock(false, logger->udata); }
}

/**
 * @brief 获取日志级别对应的字符串名称
 * @param level 日志级别枚举值
 * @return const char* 如 "TRACE"、"DEBUG" 等
 */
const char* log_level_string(int level) {
  return level_strings[level];
}

/**
 * @brief 为指定 tag 的 logger 设置自定义加锁函数
 * @param tag   logger 标签
 * @param fn    加锁函数指针，lock=true 加锁，false 解锁
 * @param udata 传递给锁函数的用户数据
 */
void log_set_lock(char* tag, log_lock_func fn, void *udata) {
  struct logger_t* logger = get_logger_by_tag(tag);
  logger->lock = fn;
  logger->udata = udata;
}

/**
 * @brief 设置指定 tag 的 logger 最低输出级别，低于该级别的日志将被忽略
 * @param tag   logger 标签
 * @param level 新的最低日志级别
 */
void log_set_level(char* tag, int level) {
  struct logger_t* logger = get_logger_by_tag(tag);
  logger->level = level;
}

/**
 * @brief 设置指定 tag 的 logger 是否开启安静模式
 * @param tag    logger 标签
 * @param enable true 开启安静模式（不输出到 stderr），false 关闭
 */
void log_set_quiet(char* tag, bool enable) {
  struct logger_t* logger = get_logger_by_tag(tag);
  logger->quiet = enable;
}

/**
 * @brief 为指定 tag 的 logger 注册一个自定义回调
 * 若 logger 不存在则自动创建。回调槽满（MAX_CALLBACKS）时返回 0。
 * @param tag   logger 标签
 * @param fn    回调函数
 * @param udata 传递给回调的用户数据
 * @param level 触发该回调的最低日志级别
 * @return int 成功返回 1，回调槽已满返回 0
 */
int log_add_callback(char* tag, log_func fn, void *udata, int level) {
  struct logger_t* logger = get_or_new_logger_by_tag(tag);
  /* 找到第一个空闲回调槽并注册 */
  for (int i = 0; i < MAX_CALLBACKS; i++) {
    if (!logger->callbacks[i].fn) {
      logger->callbacks[i] = (log_callback_t) { fn, udata, level };
      return 1;
    }
  }
  return 0; /* 回调槽已满 */
}

/**
 * @brief 为指定 tag 的 logger 注册文件输出回调
 * @param tag   logger 标签
 * @param file  日志文件路径（内部使用 sds 复制）
 * @param level 触发文件输出的最低日志级别
 * @return int 成功返回 1，失败返回 0
 */
int log_add_file(char* tag, char* file, int level) {
  return log_add_callback(tag, file_callback, sds_new(file), level);
}

/**
 * @brief 为指定 tag 的 logger 注册标准输出回调
 * @param tag   logger 标签
 * @param level 触发标准输出的最低日志级别
 * @return int 成功返回 1，失败返回 0
 */
int log_add_stdout(char* tag, int level) {
    return log_add_callback(tag, stdout_callback, stdout, level);
}

/**
 * @brief 初始化日志事件：填充当前时间和 udata
 * @param ev    日志事件指针
 * @param udata 传递给回调的用户数据（如 FILE*）
 */
static void init_event(log_event_t *ev, void *udata) {
  /* 获取精确到微秒的当前时间 */
  gettimeofday(&ev->time, NULL);
  ev->udata = udata;
  ev->pid = getpid();
}

/**
 * @brief 核心日志记录函数，遍历所有回调并触发符合级别的回调
 * @param tag      logger 标签，决定使用哪个 logger
 * @param level    本次日志的级别
 * @param file     源文件名
 * @param func     函数名
 * @param line     行号
 * @param fmt      格式化字符串
 * @param ...      格式化参数
 */
void log_log(char* tag, int level, char *file, const char* func, int line, char *fmt, ...) {
  /* 全局工厂未初始化时直接返回 */
  if (global_logger_factory.loggers == NULL) return;
  log_event_t ev = {
    .fmt   = fmt,
    .file  = file,
    .line  = line,
    .level = level,
    .func  = func,
  };

  struct logger_t* logger = get_logger_by_tag(tag);
  if (logger == NULL) return; /* tag 对应的 logger 不存在 */
  lock(logger);

  /* 遍历所有已注册回调，对达到级别的回调进行调用 */
  for (int i = 0; i < MAX_CALLBACKS && logger->callbacks[i].fn; i++) {
    log_callback_t *cb = &logger->callbacks[i];
    if (level >= cb->level) {
      init_event(&ev, cb->udata);  /* 填充时间和 udata */
      va_start(ev.ap, fmt);
      cb->fn(&ev);                 /* 调用具体回调（stdout/file） */
      va_end(ev.ap);
    }
  }

  unlock(logger);
}

/**
 * @brief 获取当前调用栈的符号化回溯字符串
 * 使用 backtrace() 和 backtrace_symbols() 获取调用栈，
 * 并将地址和符号信息格式化写入 backtrace_buffer。
 * @param backtrace_buffer 调用方提供的缓冲区，大小建议为 BT_BUFFER_SIZE
 * @return char* 返回填充了回溯信息的 backtrace_buffer 指针
 */
char *lbt(char* backtrace_buffer)
{
  int buffer_size = 100;
  void* buffer[buffer_size];

  /* 获取当前调用栈帧地址数组 */
  int size = backtrace(buffer, buffer_size);
  printf("\n lbt  %d\n", size);
  char **symbol_array = NULL;
  /* 将地址转换为可读的符号字符串数组 */
  symbol_array = backtrace_symbols(buffer, size);

  int offset = 0;
  for (int i = 0; i < size && offset < BT_BUFFER_SIZE - 1; i++) {
    if (i != 0) {
      offset += snprintf(backtrace_buffer + offset, BT_BUFFER_SIZE - offset, "\n");
    }
    /* 格式化写入地址，第一帧无前缀空格 */
    const char *format = (0 == i) ? "0x%lx" : " 0x%lx";
    offset += snprintf(
        backtrace_buffer + offset, BT_BUFFER_SIZE - offset, format, (buffer[i]));

    /* 若有符号信息则一并输出 */
    if (symbol_array != NULL) {
      offset += snprintf(backtrace_buffer + offset, BT_BUFFER_SIZE - offset, " %s", symbol_array[i]);
    }
  }

  if (symbol_array != NULL) {
    free(symbol_array);
  }
  return backtrace_buffer;
}

