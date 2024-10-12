#ifndef __LATTE_LOG_H
#define __LATTE_LOG_H

#include <stdio.h>
#include <sds/sds.h>
#include <dict/dict.h>

#include <stdarg.h>
#include <stdbool.h>
#include <time.h>
#include "zmalloc/zmalloc.h"

#define LOG_VERSION "0.0.1"

/* Log levels */
#define LL_DEBUG 0
#define LL_INFO 1
#define LL_WARN 2
#define LL_ERROR 3
#define LL_FATAL 4
#define LL_RAW (1<<10) /* Modifier to log without timestamp */


/*
 * priorities/facilities are encoded into a single 32-bit quantity, where the
 * bottom 3 bits are the priority (0-7) and the top 28 bits are the facility
 * (0-big number).  Both the priorities and the facilities map roughly
 * one-to-one to strings in the syslogd(8) source code.  This mapping is
 * included in this file.
 *
 * priorities (these are ordered)
 */
// #define LOG_EMERG       0       /* system is unusable */
// #define LOG_ALERT       1       /* action must be taken immediately */
// #define LOG_CRIT        2       /* critical conditions */
// #define LOG_ERR         3       /* error conditions */
// #define LOG_WARNING     4       /* warning conditions */
// #define LOG_NOTICE      5       /* normal but significant condition */
// #define LOG_INFO        6       /* informational */
// #define LOG_DEBUG       7       /* debug-level messages */
enum { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };

#define LOG_MAX_LEN    1024 /* Default maximum length of syslog messages.*/


#define MAX_LOGGER 4;




typedef struct {
  va_list ap;
  const char *fmt;
  const char *file;
  struct tm *time;
  void *udata;
  int line;
  int level;
  const char* func;
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
    dict* loggers;
} global_logger_factory;

void log_init();



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

void log_log(char* tag, int level, char *file, const char* function, int line, char *fmt, ...);

#define LATTE_LIB "latte_lib" 
#define LATTE_LIB_LOG(level, ...) log_log(LATTE_LIB, level, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__) 


#define BT_BUFFER_SIZE 8192
char *lbt(char* backtrace_buffer);
#endif