#include "level_db.h"
#include "../sds.h"
#include <stdio.h>

int main() {
    sds path = sdsnew("db1");
    sds key = sdsnew("mykv");
    sds value = sdsnew("value");
    void* db = level_db_open(path);
    if(level_db_set(db, key, value)) {
        printf("set err \n");
    }
    sds v = level_db_get(db, key);
    if(v == NULL) {
        printf("get err \n");
    }
    printf("get value :%s \n", v);
}