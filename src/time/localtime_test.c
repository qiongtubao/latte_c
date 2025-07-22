
#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <unistd.h>
#include "localtime.h"
#include <stdio.h>

int test_nolocks_localtime() {
    /* Obtain timezone and daylight info. */
    tzset(); /* Now 'timezome' global is populated. */
    time_t t = time(NULL);
    int daylight_active = get_daylight_active(0);

    struct tm tm;
    char buf[1024];

    nolocks_localtime(&tm,t,get_time_zone(),daylight_active);
    strftime(buf,sizeof(buf),"%d %b %H:%M:%S",&tm);
    printf("[timezone: %d, dl: %d] %s\n", (int)get_time_zone(), (int)daylight_active, buf);
    return 1;
}


int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("about nolocks_localtime function", 
            test_nolocks_localtime() == 1);
        
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}