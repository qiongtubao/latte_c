#include "hash_set.h"
#include "zmalloc/zmalloc.h"
#include "dict/dict_plugins.h"
/* hash_set dictionary type. Keys are SDS strings, values are ot used. */
hash_set_func_t sds_hash_set_dict_func = {
    dict_sds_hash,               /* hash function */
    dict_sds_dup,                      /* key dup */
    NULL,                      /* val dup */
    dict_sds_key_compare,         /* key compare */
    dict_sds_destructor,         /* key destructor */
    NULL                       /* val destructor */
};

void private_hash_set_init(hash_set_t* s, dict_func_t* dt) {
    _dict_init(s, dt);
}

hash_set_t* private_hash_set_new(hash_set_func_t* dt) {
    hash_set_t* s = zmalloc(sizeof(hash_set_t));
    private_hash_set_init(s, dt);
    return s;
}

void private_hash_set_delete(hash_set_t* hash_set) {
    dict_delete(hash_set);
}


void private_hash_set_destroy(set_t* hash_set) {
    dict_destroy((hash_set_t*)hash_set);
}

/**
 *  return 
 *  0 表示原来已经有了
 *  1 表示新加
 */
int private_hash_set_add(hash_set_t* hash_set, void* element) {
    dict_entry_t*de = dict_add_raw(hash_set,element,NULL);
    if (de) {
        dict_set_val(hash_set,de,NULL);
        return 1;
    }
    return 0;
}
/**
 *  1 删除成功
 *  0 删除失败
 */
int private_hash_set_remove(hash_set_t* hash_set, void* element) {
    if (dict_delete_key(hash_set ,element) == DICT_OK) {
        if (ht_needs_resize(hash_set)) dict_resize(hash_set);
        return 1;
    }
    return 0;
}
/**
 * 
 * 1 查找成功
 * 0 查找失败
 */
int private_hash_set_contains(hash_set_t* hash_set, void* element) {
    return dict_find(hash_set, element) != NULL;
}

size_t private_hash_set_size(hash_set_t* hash_set) {
    return dict_size(hash_set);
}


// =========
int hash_set_add_func(set_t* set, void* key) {
    return private_hash_set_add(set->data, key);
}

int hash_set_contains_func(set_t* set, void* key) {
    return private_hash_set_contains(set->data, key);
}

size_t hash_set_size_func(set_t* set) {
    return private_hash_set_size(set->data);
}

int hash_set_remove_func(set_t* set, void* key) {
    return private_hash_set_remove(set->data, key);
}

void hash_set_delete_func(set_t* set) {
    private_hash_set_delete(set->data);
    zfree(set);
}


latte_iterator_t* hash_set_get_iteratro_func(set_t* set) {
    return dict_get_latte_iterator((dict_t*)set->data);
}

set_func_t hash_set_func = {
    .add = hash_set_add_func,
    .contains = hash_set_contains_func,
    .size = hash_set_size_func,
    .remove = hash_set_remove_func,
    .release = hash_set_delete_func,
    .getIterator = hash_set_get_iteratro_func,
    .clear = private_hash_set_destroy 
};
set_t* hash_set_new(hash_set_func_t* type) {
    set_t* s = zmalloc(sizeof(set_t));
    s->data = private_hash_set_new(type);
    s->type = &hash_set_func;
    return s;
}