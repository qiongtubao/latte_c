#include "dict/dict.h"
#include "dict_plugins.h"
/* -------------------------- hash functions -------------------------------- */

static uint8_t dict_hash_function_seed[16];

uint64_t dictSdsHash(const void *key) {
    return dictGenHashFunction((unsigned char*)key, sdslen((char*)key));
}