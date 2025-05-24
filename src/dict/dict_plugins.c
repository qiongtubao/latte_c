#include "dict/dict.h"
#include "dict_plugins.h"
#include "sds/sds.h"
#include <string.h>
/* -------------------------- hash functions -------------------------------- */


uint64_t dict_sds_hash(const void *key) {
    return dict_gen_hash_function((unsigned char*)key, sds_len((char*)key));
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


static uint8_t dict_hash_function_seed[16];


#define U32TO8_LE(p, v)                                                        \
    (p)[0] = (uint8_t)((v));                                                   \
    (p)[1] = (uint8_t)((v) >> 8);                                              \
    (p)[2] = (uint8_t)((v) >> 16);                                             \
    (p)[3] = (uint8_t)((v) >> 24);

#define U64TO8_LE(p, v)                                                        \
    U32TO8_LE((p), (uint32_t)((v)));                                           \
    U32TO8_LE((p) + 4, (uint32_t)((v) >> 32));

#ifdef UNALIGNED_LE_CPU
#define U8TO64_LE(p) (*((uint64_t*)(p)))
#else
#define U8TO64_LE(p)                                                           \
    (((uint64_t)((p)[0])) | ((uint64_t)((p)[1]) << 8) |                        \
     ((uint64_t)((p)[2]) << 16) | ((uint64_t)((p)[3]) << 24) |                 \
     ((uint64_t)((p)[4]) << 32) | ((uint64_t)((p)[5]) << 40) |                 \
     ((uint64_t)((p)[6]) << 48) | ((uint64_t)((p)[7]) << 56))
#endif

#define U8TO64_LE_NOCASE(p)                                                    \
    (((uint64_t)(siptlw((p)[0]))) |                                           \
     ((uint64_t)(siptlw((p)[1])) << 8) |                                      \
     ((uint64_t)(siptlw((p)[2])) << 16) |                                     \
     ((uint64_t)(siptlw((p)[3])) << 24) |                                     \
     ((uint64_t)(siptlw((p)[4])) << 32) |                                              \
     ((uint64_t)(siptlw((p)[5])) << 40) |                                              \
     ((uint64_t)(siptlw((p)[6])) << 48) |                                              \
     ((uint64_t)(siptlw((p)[7])) << 56))


uint64_t dict_sds_case_hash_function(const unsigned char *buf, int len) {
    return siphash_nocase(buf,len,dict_hash_function_seed);
}

uint64_t dict_sds_case_hash(const void *key) {
    return dict_sds_case_hash_function((unsigned char*)key, sds_len((char*)key));
}

int dict_sds_key_case_compare(dict_t*privdata, const void *key1,
        const void *key2) {
    DICT_NOTUSED(privdata);

    return strcasecmp(key1, key2) == 0;        
}