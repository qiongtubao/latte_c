#include "dict/dict.h"
#include "dict_plugins.h"
#include "sds/sds.h"
#include <string.h>
/* -------------------------- hash functions -------------------------------- */


uint64_t dict_sds_hash(const void *key) {
    return dict_gen_hash_function((unsigned char*)key, sds_len((char*)key));
}

uint64_t dict_sds_case_hash(const void *key) {
    return dict_gen_case_hash_function((unsigned char*)key, sds_len((char*)key));
}

int dict_sds_key_compare(dict_t*privdata, const void *key1,
        const void *key2)
{
    int l1,l2;
    DICT_NOTUSED(privdata);

    l1 = sds_len((sds)key1);
    l2 = sds_len((sds)key2);
    if (l1 != l2) return 0;
    return memcmp(key1, key2, l1) == 0;
}

int dict_sds_key_case_compare(dict_t*privdata, const void *key1,
    const void *key2) {
    DICT_NOTUSED(privdata);
    return strcasecmp(key1, key2) == 0;
}

void dict_sds_destructor(dict_t*privdata, void *val) {
    DICT_NOTUSED(privdata);

    sds_delete(val);
}


#define UNUSED(x) (void)(x)
void *dict_sds_dup(dict_t*d, const void *key) {
    UNUSED(d);
    return sds_dup((const sds) key);
}


uint64_t dict_char_hash(const void *key) {
    return dict_gen_hash_function((unsigned char*)key, strlen((char*)key));
}


int dict_char_key_compare(dict_t* privdata, const void *key1,
    const void *key2) {
    int l1, l2;
    DICT_NOTUSED(privdata);
    l1 = strlen(key1);
    l2 = strlen(key2);
    if (l1 != l2) return 0;
    return memcmp(key1, key2, l1) == 0;
}
uint64_t dict_ptr_hash(const void *key) {
    return dict_gen_hash_function(key, sizeof(long));
}
int dict_ptr_key_compare(dict_t* privdata, const void *key1,
        const void *key2) {
    UNUSED(privdata);
    return key1 == key2;
}

dict_func_t sds_key_set_dict_type = {
        dict_sds_hash,
        NULL,
        NULL,
        dict_sds_key_compare,
        dict_sds_destructor,
        NULL
};