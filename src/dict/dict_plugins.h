
#ifndef __LATTE_DICT_PLUGINS_H
#define __LATTE_DICT_PLUGINS_H
#include <stdio.h>
#include <stdlib.h>
#include "dict.h"
/* key sds_t*/
uint64_t dict_sds_hash(const void *key);
int dict_sds_key_compare(dict_t*privdata, const void *key1,
        const void *key2);
void dict_sds_destructor(dict_t*privdata, void *val);
void *dict_sds_dup(dict_t*d, const void *key);

/** key char hash */
uint64_t dict_char_hash(const void *key);
int dict_char_key_compare(dict_t* privdata, const void *key1,
    const void *key2);

/**  key void* */
uint64_t dict_ptr_hash(const void *key);
int dict_ptr_key_compare(dict_t*privdata, const void *key1,
        const void *key2);

#endif