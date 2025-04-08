#ifndef __LATTE_OBJECT_SET_H
#define __LATTE_OBJECT_SET_H

#include "object.h"
#include "dict/dict.h"

latte_object_t* hash_set_object_new(dict_t* set);
latte_object_t* intset_object_new();
#endif