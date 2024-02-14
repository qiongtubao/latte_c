#ifndef __LATTE_LOG_H
#define __LATTE_LOG_H

#include <stdio.h>
#include <sds/sds.h>
/* Log levels */
#define LL_DEBUG 0
#define LL_VERBOSE 1
#define LL_NOTICE 2
#define LL_WARNING 3
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
#define LOG_EMERG       0       /* system is unusable */
#define LOG_ALERT       1       /* action must be taken immediately */
#define LOG_CRIT        2       /* critical conditions */
#define LOG_ERR         3       /* error conditions */
#define LOG_WARNING     4       /* warning conditions */
#define LOG_NOTICE      5       /* normal but significant condition */
#define LOG_INFO        6       /* informational */
#define LOG_DEBUG       7       /* debug-level messages */

/** 
 * 问题
 * 1. 时间戳引用
 * 
 * 
 */
static int logLevel = LL_DEBUG;
static sds logFilename = NULL;

void setLogLevel(int level);
int getLogLevel();
void setLogFile(const char* filename);
sds getLogFile();

void _Log(int level, const char *fmt, ...);
/* Use macro for checking log level to avoid evaluating arguments in cases log
 * should be ignored due to low level. */
#define Log(level, ...) do {\
        if (((level)&0xff) < logLevel) break;\
        _Log(level, __VA_ARGS__);\
    } while(0)




#endif