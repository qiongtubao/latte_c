#include "dict/dict.h"
#include "dict_plugins.h"
#include "sds/sds.h"
#include <string.h>
/* -------------------------- hash functions -------------------------------- */

static uint8_t dict_hash_function_seed[16];

uint64_t dictSdsHash(const void *key) {
    return dictGenHashFunction((unsigned char*)key, sdslen((char*)key));
}

int dictSdsKeyCompare(void *privdata, const void *key1,
        const void *key2)
{
    int l1,l2;
    DICT_NOTUSED(privdata);

    l1 = sdslen((sds)key1);
    l2 = sdslen((sds)key2);
    if (l1 != l2) return 0;
    return memcmp(key1, key2, l1) == 0;
}

void dictSdsDestructor(void *privdata, void *val) {
    DICT_NOTUSED(privdata);

    sdsfree(val);
}


#define UNUSED(x) (void)(x)
void *dictSdsDup(dict *d, const void *key) {
    UNUSED(d);
    return sdsdup((const sds) key);
}


uint64_t dictCharHash(const void *key) {
    return dictGenHashFunction((unsigned char*)key, strlen((char*)key));
}


int dictCharKeyCompare(void* privdata, const void *key1,
    const void *key2) {
    int l1, l2;
    DICT_NOTUSED(privdata);
    l1 = strlen(key1);
    l2 = strlen(key2);
    if (l1 != l2) return 0;
    return memcmp(key1, key2, l1) == 0;
}

