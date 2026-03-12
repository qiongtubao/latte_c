/**
 * @file dict_plugins.h
 * @brief 字典常用哈希函数与比较函数插件集合
 *        为 dict_t 提供基于 sds、char*、void* 等常见键类型的
 *        哈希函数、键比较函数、键复制/析构函数，以及预定义的 dict_func_t 实例。
 */
#ifndef __LATTE_DICT_PLUGINS_H
#define __LATTE_DICT_PLUGINS_H
#include <stdio.h>
#include <stdlib.h>
#include "dict.h"

/* ---- sds 键类型支持 ---- */

/**
 * @brief 计算 sds 键的哈希值（区分大小写）
 * @param key sds 键指针（const void*）
 * @return 64 位哈希值
 */
uint64_t dict_sds_hash(const void *key);

/**
 * @brief 计算 sds 键的哈希值（不区分大小写）
 * @param key sds 键指针（const void*）
 * @return 64 位大小写不敏感哈希值
 */
uint64_t dict_sds_case_hash(const void *key);

/**
 * @brief 比较两个 sds 键（区分大小写）
 * @param privdata dict_t 指针（未使用，符合 dict_func_t 接口）
 * @param key1     第一个 sds 键（const void*）
 * @param key2     第二个 sds 键（const void*）
 * @return 相等返回 1；不等返回 0
 */
int dict_sds_key_compare(dict_t *privdata, const void *key1,
        const void *key2);

/**
 * @brief 比较两个 sds 键（不区分大小写）
 * @param privdata dict_t 指针（未使用，符合 dict_func_t 接口）
 * @param key1     第一个 sds 键（const void*）
 * @param key2     第二个 sds 键（const void*）
 * @return 相等返回 1；不等返回 0
 */
int dict_sds_key_case_compare(dict_t *privdata, const void *key1,
        const void *key2);

/**
 * @brief 释放 sds 键（调用 sdsfree）
 * @param privdata dict_t 指针（未使用）
 * @param val      要释放的 sds 键指针
 */
void dict_sds_destructor(dict_t *privdata, void *val);

/**
 * @brief 复制 sds 键（调用 sdsdup）
 * @param d   dict_t 指针（未使用）
 * @param key 原始 sds 键指针（const void*）
 * @return 新建的 sds 副本指针
 */
void *dict_sds_dup(dict_t *d, const void *key);

/* ---- char* 键类型支持 ---- */

/**
 * @brief 计算 char* 键的哈希值（以 '\0' 结尾的 C 字符串）
 * @param key C 字符串指针（const void*）
 * @return 64 位哈希值
 */
uint64_t dict_char_hash(const void *key);

/**
 * @brief 比较两个 char* 键（strcmp 比较）
 * @param privdata dict_t 指针（未使用）
 * @param key1     第一个 C 字符串键
 * @param key2     第二个 C 字符串键
 * @return 相等返回 1；不等返回 0
 */
int dict_char_key_compare(dict_t *privdata, const void *key1,
    const void *key2);

/* ---- void* 指针键类型支持 ---- */

/**
 * @brief 计算 void* 指针键的哈希值（基于指针地址）
 * @param key 任意指针（const void*）
 * @return 64 位哈希值
 */
uint64_t dict_ptr_hash(const void *key);

/**
 * @brief 比较两个 void* 指针键（直接比较指针值）
 * @param privdata dict_t 指针（未使用）
 * @param key1     第一个指针键
 * @param key2     第二个指针键
 * @return 相等返回 1；不等返回 0
 */
int dict_ptr_key_compare(dict_t *privdata, const void *key1,
        const void *key2);



/**
 * @brief 以 sds 为键的集合（Set）字典类型实例
 *        使用 sds 哈希、sds 比较（区分大小写）、sds 复制/析构，值不复制/析构。
 *        可用于实现 key-only 的集合结构（值设为 NULL）。
 */
extern dict_func_t sds_key_set_dict_type;
#endif
