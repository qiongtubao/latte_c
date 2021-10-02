

#include <stdint.h>
#include <string.h>
#include "sds.h"
typedef long long mstime_t; /* millisecond time type. */
typedef long long ustime_t; /* microsecond time type. */
#if defined(__linux__)
#include <linux/time.h>
#else 
#include <time.h>
#endif
#define UNUSED(x) (void)(x)
int ll2string(char *s, size_t len, long long value);
int string2ll(const char *s, size_t slen, long long *value);
int d2string(char *buf, size_t len, double value);

int ld2string(char *buf, size_t len, long double value, int humanfriendly);
int string2ld(const char *s, size_t slen, long double *dp);
int string2d(const char*s, size_t slen, double* dp);
unsigned long long ustime(void);
unsigned long long mstime(void);
unsigned long long nstime(void);
sds getAbsolutePath(char* filename);

#define BIG 1
#define LITTLE 0
int get_big_or_little();
/* We use a private localtime implementation which is fork-safe. The logging
 * function of Redis may be called from other threads. */
void nolocks_localtime(struct tm *tmp, time_t t, time_t tz, int dst);
void getRandomBytes(unsigned char *p, size_t len);
