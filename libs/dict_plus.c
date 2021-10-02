#include "dict_plus.h"
#include <string.h>
/* Generic hash function (a popular one from Bernstein).
 * I tested a few and this was the best. */

uint64_t dictSdsHash(const void *key) {
    return dictGenHashFunction((unsigned char*)key, sdslen((char*)key));
}

uint64_t dictSdsCaseHash(const void *key) {
    return dictGenCaseHashFunction((unsigned char*)key, sdslen((char*)key));
}

int dictSdsKeyCaseCompare(void *privdata, const void *key1,
        const void *key2) {
    NOTUSED(privdata);
    return strcasecmp(key1, key2) == 0;
}

int dictSdsKeyCompare(void *privdata, const void *key1,
        const void *key2) {
    int l1,l2;
    DICT_NOTUSED(privdata);

    l1 = sdslen((sds)key1);
    l2 = sdslen((sds)key2);
    if (l1 != l2) return 0;
    return memcmp(key1, key2, l1) == 0;
}

void dictSdsDestructor(void *privdata, void *val) {
    NOTUSED(privdata);
    if (val == NULL) return; /* Lazy freeing will set value to NULL. */
    sdsfree(val);
}
