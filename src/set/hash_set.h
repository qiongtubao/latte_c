
#ifndef __LATTE_HASH_SET_H
#define __LATTE_HASH_SET_H

#include <dict/dict.h>
#include <dict/dict_plugins.h>
#include "set.h"

typedef struct dict_t hash_set_t;
typedef struct dict_func_t hash_set_func_t; 
extern hash_set_func_t sds_hash_set_type;

// hash_set_t* private_hash_set_new(hash_set_func_t* dt);
// void private_hash_set_delete(hash_set_t* hashSet);

// void private_hash_set_init(hash_set_t* hashSet, hash_set_func_t* dt);
// void private_hash_set_destroy(hash_set_t* hashSet);


// int private_hash_set_add(hash_set_t* hashSet, void* element);
// int patvate_hash_set_remove(hash_set_t* hashSet, void* element);
// int private_hash_set_contains(hash_set_t* hashSet, void* element);

// size_t private_hash_set_size(hash_set_t* hashSet);


//set api
set_t* hash_set_new(hash_set_func_t* type);
extern  hash_set_func_t sds_hash_set_dict_func;
#endif