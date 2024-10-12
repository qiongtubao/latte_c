
#ifndef __LATTE_DICT_PLUGINS_H
#define __LATTE_DICT_PLUGINS_H
#include <stdio.h>
#include <stdlib.h>
#include "dict.h"
/* key sds_t*/
uint64_t dictSdsHash(const void *key);
int dictSdsKeyCompare(dict *privdata, const void *key1,
        const void *key2);
void dictSdsDestructor(dict *privdata, void *val);
void *dictSdsDup(dict *d, const void *key);

uint64_t dictCharHash(const void *key);
int dictCharKeyCompare(dict* privdata, const void *key1,
    const void *key2);
#endif