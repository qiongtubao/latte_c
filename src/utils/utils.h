#ifndef __LATTE_UTILS_H
#define __LATTE_UTILS_H

#include <stdio.h>
#include <math.h>
#include <stdint.h>


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

int ll2string(char *s, size_t len, long long value);
int string2ll(const char *s, size_t slen, long long *value);
// int string2ll(const char *s, size_t slen, long long *value);
// int string2ull(const char *s, unsigned long long *value);
// int string2l(const char *s, size_t slen, long *value);
// int string2ld(const char *s, size_t slen, long double *dp);
// int string2d(const char *s, size_t slen, double *dp);
// int d2string(char *buf, size_t len, double value);
long long ustime(void);

static long long nowustime;
static long daylight_active;
static int start_update_cache_timed = 0;
long getTimeZone();
long getDaylightActive();
long updateDaylightActive();
#endif