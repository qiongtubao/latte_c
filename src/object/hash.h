

#ifndef __LATTE_OBJECT_HASH_H
#define __LATTE_OBJECT_HASH_H
#include "dict/dict.h"
#include "object.h"
latte_object_t* hash_object_new(dict_t* d);
latte_object_t* ziplist_hash_object_new();
#endif

