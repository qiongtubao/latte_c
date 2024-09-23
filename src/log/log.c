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
#include <string.h>

struct Logger* getLoggerByTag(char* tag) {
  dictEntry* entry = dictFind(loggerFactory.loggers, tag);
  if (entry == NULL) return NULL;
  return dictGetVal(entry);
}

struct Logger* loggerCreate() {
  struct Logger* logger = zmalloc(sizeof(struct Logger));
  return logger; 
}

struct Logger* getOrCreateLoggerByTag(char* tag) {
    struct Logger* logger = getLoggerByTag(tag);
    if (logger == NULL) {
        logger = loggerCreate();
        dictAdd(loggerFactory.loggers, tag, logger);
    }
    return logger;
}

void loggerFree(struct Logger* logger) {
    zfree(logger);
}

uint64_t LoggerHashCallback(const void *key) {
    return dictGenCaseHashFunction((unsigned char*)key, strlen((char*)key));
}

int LoggerCompareCallback(dict *privdata, const void *key1, const void *key2) {
    int l1,l2;
    DICT_NOTUSED(privdata);
    l1 = strlen((char*) key1);
    l2 = strlen((char*) key2);
    if(l1 != l2) return 0;
    return memcmp(key1, key2,l1) == 0;
}



void LoggerFreeCallback(dict* privdata, void* val) {
    DICT_NOTUSED(privdata);
    // printf("delete :%p\n", val);
    if(val != NULL) {
        loggerFree(val);
    }
}
static dictType LoggerDict = {
    LoggerHashCallback,
    NULL,
    NULL,
    LoggerCompareCallback,
    LoggerFreeCallback,
    NULL,
    NULL
};
void initLogger() {
  loggerFactory.loggers = dictCreate(&LoggerDict);
}

static const char *level_strings[] = {
  "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

#ifdef LOG_USE_COLOR
static const char *level_colors[] = {
  "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"
};
#endif


static void stdout_callback(log_Event *ev) {
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


static void file_callback(log_Event *ev) {
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


static void lock(struct Logger* logger)   {
  if (logger->lock) { logger->lock(true, logger->udata); }
}


static void unlock(struct Logger* logger) {
  if (logger->lock) { logger->lock(false, logger->udata); }
}


const char* log_level_string(int level) {
  return level_strings[level];
}


void log_set_lock(char* tag, log_LockFn fn, void *udata) {
  struct Logger* logger = getLoggerByTag(tag);
//   assert(logger != NULL);
  logger->lock = fn;
  logger->udata = udata;
}


void log_set_level(char* tag, int level) {
  struct Logger* logger = getLoggerByTag(tag);
//   assert(logger != NULL);
  logger->level = level;
}


void log_set_quiet(char* tag, bool enable) {
  struct Logger* logger = getLoggerByTag(tag);
//   assert(logger != NULL);
  logger->quiet = enable;
}



int log_add_callback(char* tag, log_LogFn fn, void *udata, int level) {
  struct Logger* logger = getOrCreateLoggerByTag(tag);
  for (int i = 0; i < MAX_CALLBACKS; i++) {
    if (!logger->callbacks[i].fn) {
      logger->callbacks[i] = (Callback) { fn, udata, level };
      return 1;
    }
  }
  return 0;
}


int log_add_file(char* tag, char* file, int level) {
  return log_add_callback(tag, file_callback, sdsnew(file), level);
}

int log_add_stdout(char* tag, int level) {
    return log_add_callback(tag, stdout_callback, stdout, level);
}


static void init_event(log_Event *ev, void *udata) {
  if (!ev->time) {
    time_t t = time(NULL);
    ev->time = localtime(&t);
  }
  ev->udata = udata;
}


void log_log(char* tag, int level, char *file, char* func, int line, char *fmt, ...) {
  if (loggerFactory.loggers == NULL) return;
  log_Event ev = {
    .fmt   = fmt,
    .file  = file,
    .line  = line,
    .level = level,
    .func = func,
  };
 
  struct Logger* logger = getLoggerByTag(tag);
  if (logger == NULL) return;
  lock(logger);

//   if (!logger->quiet && level >= logger->level) {
//     init_event(&ev, stderr);
//     va_start(ev.ap, fmt);
//     stdout_callback(&ev);
//     va_end(ev.ap);
//   }

  for (int i = 0; i < MAX_CALLBACKS && logger->callbacks[i].fn; i++) {
    Callback *cb = &logger->callbacks[i];
    if (level >= cb->level) {
      init_event(&ev, cb->udata);
      va_start(ev.ap, fmt);
      cb->fn(&ev);
      va_end(ev.ap);
    }
  }

  unlock(logger);
}
