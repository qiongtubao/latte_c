#ifndef __LATTE_UTILS_H
#define __LATTE_UTILS_H

#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "sds/sds.h"

#define MAX_LONG_DOUBLE_CHARS 5*1024

#ifdef __GNUC__
#  define likely(x)   __builtin_expect(!!(x), 1)
#  define unlikely(x) __builtin_expect(!!(x), 0)
#else
#  define likely(x)   !!(x)
#  define unlikely(x) !!(x)
#endif

#if __GNUC__ >= 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
#define latte_unreachable __builtin_unreachable
#else
#define latte_unreachable abort
#endif


// 检查并打印错误信息
void latte_assert(int condition, const char *message, ...);

int ll2string(char *s, size_t len, long long value);
sds ll2sds(long long value);
int string2ll(const char *s, size_t slen, long long *value);
int sds2ll(sds s, long long* value);
// int string2ll(const char *s, size_t slen, long long *value);
int string2ull(const char *s, unsigned long long *value);
int ull2string(char *s, size_t len, unsigned long long* value);
// int string2l(const char *s, size_t slen, long *value);
int string2ld(const char *s, size_t slen, long double *dp);
int sds2ld(sds s, long double* dp);
/* long double to string convertion options */
typedef enum {
    LD_STR_AUTO,     /* %.17Lg */
    LD_STR_HUMAN,    /* %.17Lf + Trimming of trailing zeros */
    LD_STR_HEX       /* %La */
} ld2string_mode;
int ld2string(char *buf, size_t len, long double value, ld2string_mode mode);
sds ld2sds(long double value, ld2string_mode mode);
int string2d(const char *s, size_t slen, double *dp);
int sds2d(sds value, double* dp);
int d2string(char *buf, size_t len, double value);
sds d2sds(double value);
long long ustime(void);

static long long nowustime;
static long daylight_active;
static int start_update_cache_timed = 0;
long getTimeZone();
long getDaylightActive();
long updateDaylightActive();
#endif