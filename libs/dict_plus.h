#include "sds.h"
#include "dict.h"
#define NOTUSED(V) ((void) V)
//about sds
uint64_t dictSdsHash(const void *key);
uint64_t dictSdsCaseHash(const void *key);
int dictSdsKeyCaseCompare(void *privdata, const void *key1,
        const void *key2);
void dictSdsDestructor(void *privdata, void *val);
int dictSdsKeyCompare(void *privdata, const void *key1,
        const void *key2);
        