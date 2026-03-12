
#ifndef __LATTE_HASH_SET_H
#define __LATTE_HASH_SET_H

#include <dict/dict.h>
#include <dict/dict_plugins.h>
#include "set.h"

/**
 * @brief 使用字典来实现HashSet
 */
typedef struct dict_t hash_set_t;

/**
 * @brief HashSet的函数表，实际是复用字典的函数表
 */
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
/**
 * @brief 创建一个新的基于哈希表实现的Set
 * @param type 哈希表的具体类型操作函数表
 * @return set_t* 返回抽象的Set对象
 */
set_t* hash_set_new(hash_set_func_t* type);

/**
 * @brief 键为sds字符串的HashSet字典操作函数表实例
 */
extern hash_set_func_t sds_hash_set_dict_func;
#endif