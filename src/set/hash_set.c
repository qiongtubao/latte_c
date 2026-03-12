#include "hash_set.h"
#include "zmalloc/zmalloc.h"
#include "dict/dict_plugins.h"

/**
 * @brief sds类型作为键的hash_set字典操作函数表
 * hash_set内部其实就是一个没有value的字典，所以value相关的函数全为NULL
 */
/* hash_set dictionary type. Keys are SDS strings, values are ot used. */
hash_set_func_t sds_hash_set_dict_func = {
    dict_sds_hash,               /* hash function */
    dict_sds_dup,                      /* key dup */
    NULL,                      /* val dup */
    dict_sds_key_compare,         /* key compare */
    dict_sds_destructor,         /* key destructor */
    NULL                       /* val destructor */
};

/**
 * @brief 初始化hash_set底层结构（字典）
 * @param s 要初始化的hash_set指针
 * @param dt 字典操作函数表
 */
void private_hash_set_init(hash_set_t* s, dict_func_t* dt) {
    _dict_init(s, dt);
}

/**
 * @brief 创建一个新的hash_set底层结构
 * @param dt 字典操作函数表
 * @return hash_set_t* 返回创建的hash_set
 */
hash_set_t* private_hash_set_new(hash_set_func_t* dt) {
    hash_set_t* s = zmalloc(sizeof(hash_set_t));
    private_hash_set_init(s, dt);
    return s;
}

/**
 * @brief 删除并释放hash_set底层结构
 * @param hash_set 要释放的hash_set
 */
void private_hash_set_delete(hash_set_t* hash_set) {
    dict_delete(hash_set);
}

/**
 * @brief 销毁/清空hash_set底层结构的数据，但不释放结构体本身
 * @param hash_set 要清空的hash_set（强转为set_t*）
 */
void private_hash_set_destroy(set_t* hash_set) {
    dict_destroy((hash_set_t*)hash_set);
}

/**
 * @brief 向hash_set底层结构中添加元素
 * @param hash_set 目标hash_set
 * @param element 要添加的元素
 * @return int 0 表示元素已存在，1 表示新添加成功
 */
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
 * @brief 从hash_set底层结构中移除元素
 * @param hash_set 目标hash_set
 * @param element 要移除的元素
 * @return int 1 删除成功，0 删除失败（元素不存在）
 */
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
 * @brief 检查hash_set底层结构中是否包含元素
 * @param hash_set 目标hash_set
 * @param element 要检查的元素
 * @return int 1 查找成功（存在），0 查找失败（不存在）
 */
/**
 *
 * 1 查找成功
 * 0 查找失败
 */
int private_hash_set_contains(hash_set_t* hash_set, void* element) {
    return dict_find(hash_set, element) != NULL;
}

/**
 * @brief 获取hash_set底层结构的元素数量
 * @param hash_set 目标hash_set
 * @return size_t 元素数量
 */
size_t private_hash_set_size(hash_set_t* hash_set) {
    return dict_size(hash_set);
}


// ========= 以下是多态接口的实现函数包装 =========

/**
 * @brief 实现 set_func_t 的 add 接口
 */
int hash_set_add_func(set_t* set, void* key) {
    return private_hash_set_add(set->data, key);
}

/**
 * @brief 实现 set_func_t 的 contains 接口
 */
int hash_set_contains_func(set_t* set, void* key) {
    return private_hash_set_contains(set->data, key);
}

/**
 * @brief 实现 set_func_t 的 size 接口
 */
size_t hash_set_size_func(set_t* set) {
    return private_hash_set_size(set->data);
}

/**
 * @brief 实现 set_func_t 的 remove 接口
 */
int hash_set_remove_func(set_t* set, void* key) {
    return private_hash_set_remove(set->data, key);
}

/**
 * @brief 实现 set_func_t 的 release 接口
 */
void hash_set_delete_func(set_t* set) {
    private_hash_set_delete(set->data);
    zfree(set);
}

/**
 * @brief 实现 set_func_t 的 getIterator 接口
 */
latte_iterator_t* hash_set_get_iteratro_func(set_t* set) {
    return dict_get_latte_iterator((dict_t*)set->data);
}

/**
 * @brief HashSet的虚拟函数表(vtable)，实现了Set的接口
 */
set_func_t hash_set_func = {
    .add = hash_set_add_func,
    .contains = hash_set_contains_func,
    .size = hash_set_size_func,
    .remove = hash_set_remove_func,
    .release = hash_set_delete_func,
    .getIterator = hash_set_get_iteratro_func,
    .clear = private_hash_set_destroy
};

/**
 * @brief 创建一个新的基于哈希表实现的Set
 * @param type 哈希表的具体类型操作函数表
 * @return set_t* 返回抽象的Set对象
 */
set_t* hash_set_new(hash_set_func_t* type) {
    set_t* s = zmalloc(sizeof(set_t));
    s->data = private_hash_set_new(type);
    s->type = &hash_set_func;
    return s;
}