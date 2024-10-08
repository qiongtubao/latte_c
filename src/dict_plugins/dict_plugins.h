
#ifndef __LATTE_DICT_PLUGINS_H
#define __LATTE_DICT_PLUGINS_H
#include <stdio.h>
#include <stdlib.h>

/* key sds */
uint64_t dictSdsHash(const void *key);
int dictSdsKeyCompare(void *privdata, const void *key1,
        const void *key2);
void dictSdsDestructor(void *privdata, void *val);
void *dictSdsDup(dict *d, const void *key);
#endif