/**
 * @file dict_plugins.c
 * @brief 字典插件实现
 *        提供各种键类型的哈希函数、比较函数和销毁函数
 */
#include "dict/dict.h"
#include "dict_plugins.h"
#include "sds/sds.h"
#include <string.h>
/* -------------------------- 哈希函数 -------------------------------- */


/**
 * @brief SDS 字符串哈希函数
 * @param key SDS 字符串键
 * @return 哈希值
 */
uint64_t dict_sds_hash(const void *key) {
    return dict_gen_hash_function((unsigned char*)key, sds_len((char*)key));
}

/**
 * @brief SDS 字符串忽略大小写哈希函数
 * @param key SDS 字符串键
 * @return 哈希值
 */
uint64_t dict_sds_case_hash(const void *key) {
    return dict_gen_case_hash_function((unsigned char*)key, sds_len((char*)key));
}

/**
 * @brief SDS 字符串键比较函数
 * @param privdata 私有数据（未使用）
 * @param key1 第一个键
 * @param key2 第二个键
 * @return 1 表示相等，0 表示不相等
 */
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

/**
 * @brief SDS 字符串键忽略大小写比较函数
 * @param privdata 私有数据（未使用）
 * @param key1 第一个键
 * @param key2 第二个键
 * @return 1 表示相等，0 表示不相等
 */
int dict_sds_key_case_compare(dict_t*privdata, const void *key1,
    const void *key2) {
    DICT_NOTUSED(privdata);
    return strcasecmp(key1, key2) == 0;
}

/**
 * @brief SDS 字符串销毁函数
 * @param privdata 私有数据（未使用）
 * @param val 要销毁的值
 */
void dict_sds_destructor(dict_t*privdata, void *val) {
    DICT_NOTUSED(privdata);

    sds_delete(val);
}


#define UNUSED(x) (void)(x)
/**
 * @brief SDS 字符串复制函数
 * @param d   字典指针（未使用）
 * @param key 要复制的 SDS 键
 * @return 复制的 SDS 字符串
 */
void *dict_sds_dup(dict_t*d, const void *key) {
    UNUSED(d);
    return sds_dup((const sds) key);
}


/**
 * @brief C 字符串哈希函数
 * @param key C 字符串键
 * @return 哈希值
 */
uint64_t dict_char_hash(const void *key) {
    return dict_gen_hash_function((unsigned char*)key, strlen((char*)key));
}


/**
 * @brief C 字符串键比较函数
 * @param privdata 私有数据（未使用）
 * @param key1 第一个键
 * @param key2 第二个键
 * @return 1 表示相等，0 表示不相等
 */
int dict_char_key_compare(dict_t* privdata, const void *key1,
    const void *key2) {
    int l1, l2;
    DICT_NOTUSED(privdata);
    l1 = strlen(key1);
    l2 = strlen(key2);
    if (l1 != l2) return 0;
    return memcmp(key1, key2, l1) == 0;
}

/**
 * @brief 指针哈希函数
 * @param key 指针键
 * @return 哈希值
 */
uint64_t dict_ptr_hash(const void *key) {
    return dict_gen_hash_function(key, sizeof(long));
}

/**
 * @brief 指针键比较函数
 * @param privdata 私有数据（未使用）
 * @param key1 第一个键
 * @param key2 第二个键
 * @return 1 表示相等，0 表示不相等
 */
int dict_ptr_key_compare(dict_t* privdata, const void *key1,
        const void *key2) {
    UNUSED(privdata);
    return key1 == key2;
}

/**< SDS 键集合字典类型定义 */
dict_func_t sds_key_set_dict_type = {
        dict_sds_hash,
        NULL,
        NULL,
        dict_sds_key_compare,
        dict_sds_destructor,
        NULL
};