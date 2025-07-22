#ifndef __LATTE_LOCALTIME_H
#define __LATTE_LOCALTIME_H
#include <time.h>

static int is_leap_year(time_t year) {
    if (year % 4) return 0;         /* A year not divisible by 4 is not leap. */
    else if (year % 100) return 1;  /* If div by 4 and not 100 is surely leap. */
    else if (year % 400) return 0;  /* If div by 100 *and* not by 400 is not leap. */
    else return 1;                  /* If div by 100 and 400 is leap. */
}


void nolocks_localtime(struct tm *tmp, time_t t, time_t tz, int dst);

#endif