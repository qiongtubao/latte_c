#ifndef __LATTE_DICT_PLUGINS_H
#define __LATTE_DICT_PLUGINS_H
#include <stdio.h>
#include <stdlib.h>
#include "dict.h"



/* The default hashing function uses SipHash implementation
 * in siphash.c. */

uint64_t siphash(const uint8_t *in, const size_t inlen, const uint8_t *k);
uint64_t siphash_nocase(const uint8_t *in, const size_t inlen, const uint8_t *k);



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

/** sds case */
uint64_t dict_sds_case_hash(const void *key);
int dict_sds_key_case_compare(dict_t*privdata, const void *key1,
        const void *key2);



/* sds key set dict type */
extern dict_func_t sds_key_set_dict_type;

/** sds case */
uint64_t dict_sds_case_hash(const void *key);
int dict_sds_key_case_compare(dict_t*privdata, const void *key1,
        const void *key2);

#endif