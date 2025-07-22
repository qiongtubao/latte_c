#ifndef __LATTE_LOCALTIME_H
#define __LATTE_LOCALTIME_H
#include <time.h>

typedef long long mstime_t; /* millisecond time type. */
typedef long long ustime_t; /* microsecond time type. */

long get_time_zone();
int get_daylight_active(time_t t);
void nolocks_localtime(struct tm *tmp, time_t t, time_t tz, int dst);
ustime_t ustime(void);
mstime_t mstime(void);
unsigned long  current_monitonic_time();


// static long long nowustime;
// static long daylight_active;
// static int start_update_cache_timed = 0;
// long get_time_zone();
// long get_day_light_active();
// long update_day_light_active();


#endif