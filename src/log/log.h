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

#define LOG_VERSION "0.0.1"




typedef enum log_level_enum {
  LOG_TRACE, 
  LOG_DEBUG, 
  LOG_INFO, 
  LOG_WARN, 
  LOG_ERROR, 
  LOG_FATAL 
} log_level_enum;


typedef struct log_event_t {
  va_list ap;
  const char *fmt;
  const char *file;
  struct timeval time;
  void *udata;
  int line;
  int level;
  const char* func;
  int pid;
} log_event_t;

typedef void (*log_func)(log_event_t *ev);
typedef void (*log_lock_func)(bool lock, void *udata);

#define MAX_CALLBACKS 32

typedef struct {
  log_func fn;
  void *udata;
  int level;
} log_callback_t;

struct logger_t {
  void *udata;
  log_lock_func lock;
  int level;
  bool quiet;
  log_callback_t callbacks[MAX_CALLBACKS];
} ;

static struct logger_factory_t {
    dict_t* loggers;
    long timezone;      //暂时把这2个属性先放这里  以后是否会移动到time模块  之后再说
    int daylight_active; 
} global_logger_factory;
void log_module_init();


void log_log(char* tag, int level, char *file, const char* function, int line, char *fmt, ...);
// enum { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };

#define log_trace(tag, ...) log_log(tag, LOG_TRACE, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define log_debug(tag, ...) log_log(tag, LOG_DEBUG, __FILE__, __FUNCTION__,  __LINE__, __VA_ARGS__)
#define log_info(tag, ...)  log_log(tag, LOG_INFO,  __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define log_warn(tag, ...)  log_log(tag, LOG_WARN,  __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define log_error(tag, ...) log_log(tag, LOG_ERROR, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define log_fatal(tag, ...) log_log(tag, LOG_FATAL, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)


const char* log_level_string(int level);
void log_set_lock(char* tag, log_lock_func fn, void *udata);
void log_set_level(char* tag, int level);
void log_set_quiet(char* tag, bool enable);
int log_add_callback(char* tag, log_func fn, void *udata, int level);
int log_add_file(char* tag, char *fp, int level);
int log_add_stdout(char* tag, int level);



#define LATTE_LIB "latte_lib" 
#define LATTE_LIB_LOG(level, ...) log_log(LATTE_LIB, level, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__) 


#define BT_BUFFER_SIZE 8192
char *lbt(char* backtrace_buffer);
#endif
