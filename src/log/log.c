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

struct logger_t* get_logger_by_tag(char* tag) {
  dictEntry* entry = dictFind(global_logger_factory.loggers, tag);
  if (entry == NULL) return NULL;
  return dictGetVal(entry);
}

struct logger_t* loggerCreate() {
  struct logger_t* logger = zmalloc(sizeof(struct logger_t));
  return logger; 
}

struct logger_t* get_or_new_logger_by_tag(char* tag) {
    struct logger_t* logger = get_logger_by_tag(tag);
    if (logger == NULL) {
        logger = loggerCreate();
        dictAdd(global_logger_factory.loggers, tag, logger);
    }
    return logger;
}

void loggerFree(struct logger_t* logger) {
    zfree(logger);
}

uint64_t logger_hash_callback(const void *key) {
    return dictGenCaseHashFunction((unsigned char*)key, strlen((char*)key));
}

int logger_compare_callback(dict *privdata, const void *key1, const void *key2) {
    int l1,l2;
    DICT_NOTUSED(privdata);
    l1 = strlen((char*) key1);
    l2 = strlen((char*) key2);
    if(l1 != l2) return 0;
    return memcmp(key1, key2,l1) == 0;
}



void logger_free_callback(dict* privdata, void* val) {
    DICT_NOTUSED(privdata);
    // printf("delete :%p\n", val);
    if(val != NULL) {
        loggerFree(val);
    }
}
static dictType logger_dict_type = {
    logger_hash_callback,
    NULL,
    NULL,
    logger_compare_callback,
    logger_free_callback,
    NULL,
    NULL
};
void log_init() {
  global_logger_factory.loggers = dictCreate(&logger_dict_type);
}

static const char *level_strings[] = {
  "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

#ifdef LOG_USE_COLOR
static const char *level_colors[] = {
  "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"
};
#endif


static void stdout_callback(log_event_t *ev) {
  char buf[16];
  buf[strftime(buf, sizeof(buf), "%H:%M:%S", ev->time)] = '\0';
#ifdef LOG_USE_COLOR
  fprintf(
    ev->udata, "%s %s%-5s\x1b[0m \x1b[90m%s:%s:%d:\x1b[0m ",
    buf, level_colors[ev->level], level_strings[ev->level],
    ev->file, ev->func, ev->line);
#else
  fprintf(
    ev->udata, "%s %-5s %s:%s:%d: ",
    buf, level_strings[ev->level], ev->file, ev->func, ev->line);
#endif
  vfprintf(ev->udata, ev->fmt, ev->ap);
  fprintf(ev->udata, "\n");
  fflush(ev->udata);
}


static void file_callback(log_event_t *ev) {
  char buf[64];
  buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ev->time)] = '\0';
  FILE* fp = fopen((sds)ev->udata, "w+");
  fprintf(
    fp, "%s %-5s %s:%s:%d: ",
    buf, level_strings[ev->level], ev->file, ev->func, ev->line);
  vfprintf(fp, ev->fmt, ev->ap);
  fprintf(fp, "\n");
  fflush(fp);
  fclose(fp);
}


static void lock(struct logger_t* logger)   {
  if (logger->lock) { logger->lock(true, logger->udata); }
}


static void unlock(struct logger_t* logger) {
  if (logger->lock) { logger->lock(false, logger->udata); }
}


const char* log_level_string(int level) {
  return level_strings[level];
}


void log_set_lock(char* tag, log_lock_func fn, void *udata) {
  struct logger_t* logger = get_logger_by_tag(tag);
//   assert(logger != NULL);
  logger->lock = fn;
  logger->udata = udata;
}


void log_set_level(char* tag, int level) {
  struct logger_t* logger = get_logger_by_tag(tag);
//   assert(logger != NULL);
  logger->level = level;
}


void log_set_quiet(char* tag, bool enable) {
  struct logger_t* logger = get_logger_by_tag(tag);
//   assert(logger != NULL);
  logger->quiet = enable;
}



int log_add_callback(char* tag, log_func fn, void *udata, int level) {
  struct logger_t* logger = get_or_new_logger_by_tag(tag);
  for (int i = 0; i < MAX_CALLBACKS; i++) {
    if (!logger->callbacks[i].fn) {
      logger->callbacks[i] = (log_callback_t) { fn, udata, level };
      return 1;
    }
  }
  return 0;
}


int log_add_file(char* tag, char* file, int level) {
  return log_add_callback(tag, file_callback, sds_new(file), level);
}

int log_add_stdout(char* tag, int level) {
    return log_add_callback(tag, stdout_callback, stdout, level);
}


static void init_event(log_event_t *ev, void *udata) {
  if (!ev->time) {
    time_t t = time(NULL);
    ev->time = localtime(&t);
  }
  ev->udata = udata;
}


void log_log(char* tag, int level, char *file, const char* func, int line, char *fmt, ...) {
  if (global_logger_factory.loggers == NULL) return;
  log_event_t ev = {
    .fmt   = fmt,
    .file  = file,
    .line  = line,
    .level = level,
    .func = func,
  };
 
  struct logger_t* logger = get_logger_by_tag(tag);
  if (logger == NULL) return;
  lock(logger);

//   if (!logger->quiet && level >= logger->level) {
//     init_event(&ev, stderr);
//     va_start(ev.ap, fmt);
//     stdout_callback(&ev);
//     va_end(ev.ap);
//   }

  for (int i = 0; i < MAX_CALLBACKS && logger->callbacks[i].fn; i++) {
    log_callback_t *cb = &logger->callbacks[i];
    if (level >= cb->level) {
      init_event(&ev, cb->udata);
      va_start(ev.ap, fmt);
      cb->fn(&ev);
      va_end(ev.ap);
    }
  }

  unlock(logger);
}


char *lbt(char* backtrace_buffer)
{
  int buffer_size = 100;
  void* buffer[buffer_size];

  // int     bt_buffer_size = 8192;
  // char backtrace_buffer[bt_buffer_size];

  int size = backtrace(buffer, buffer_size);
  printf("\n lbt  %d\n", size);
  char **symbol_array = NULL;
// #ifdef LBT_SYMBOLS
  /* 有些环境下，使用addr2line 无法根据地址输出符号 */
  symbol_array = backtrace_symbols(buffer, size);
// #endif  // LBT_SYMBOLS

  int offset = 0;
  for (int i = 0; i < size && offset < BT_BUFFER_SIZE - 1; i++) {
    if (i != 0) {
      offset += snprintf(backtrace_buffer + offset, BT_BUFFER_SIZE - offset, "\n");
    }
    const char *format = (0 == i) ? "0x%lx" : " 0x%lx";
    offset += snprintf(
        backtrace_buffer + offset, BT_BUFFER_SIZE - offset, format, (buffer[i]));

    if (symbol_array != NULL) {
      offset += snprintf(backtrace_buffer + offset, BT_BUFFER_SIZE - offset, " %s", symbol_array[i]);
    }
  }

  if (symbol_array != NULL) {
    free(symbol_array);
  }
  return backtrace_buffer;
}

