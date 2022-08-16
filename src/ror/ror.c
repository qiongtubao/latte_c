
#include "ror.h"
#include "sds/sds.h"
#include "utils/utils.h"
#include <string.h>
char* nextSpace(char* buf, int n) {
    char* result = buf;
    while(n > 0) {
        n--;
        result = strstr(result, " ");
        if (n != 0) result += 1; 
    }
    return result;
}

double str2KB(char* buf, int size) {
    char* end = strstr(buf, "M");
    double result = 0.0L;
    if (end != NULL && (end - buf) < size) {
        string2d(buf, end - buf, &result);
        result *= 1024;      
    }
    return result;
}
void db_stats_info(char* rocksdb_stats) {
    char* buf = "Cumulative writes: ";
    char* start = strstr(rocksdb_stats, buf);
    char* end = nextSpace(start + strlen(buf), 1);
    sds result = sdsnewlen(start+ strlen(buf), end - start - strlen(buf));
    
    printf("\n[%s]\n", result);
    double r = str2KB(start+strlen(buf), end-start-strlen(buf));
    printf("\n*%.3f*\n", r);

    start = nextSpace(end+1, 1) + 1;
    end = nextSpace(start, 1);
    result = sdsnewlen(start, end-start);
    printf("\n[%s]\n", result);
    r = str2KB(start, end-start);
    printf("\n*%.3f*\n", r);
}