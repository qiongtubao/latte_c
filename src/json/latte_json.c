/**
 * @file latte_json.c
 * @brief JSON数据结构操作实现模块
 *        提供JSON映射、数组的创建、数据插入、获取、删除等核心功能
 */
#include "latte_json.h"
#include "dict/dict_plugins.h"
// #include "json.h"
#include "utils/utils.h"
#include <errno.h>
#include <string.h>
#include "log/log.h"
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

// map
dict_func_t jsonDictTyep  = {
    dict_char_hash,
    NULL,
    NULL,
    dict_char_key_compare,
    dict_sds_destructor,
    NULL,
    NULL
};

/**
 * @brief 创建新的JSON映射对象
 * @return 返回初始化的JSON映射对象指针，失败返回NULL
 */
json_t* json_map_new() {
    json_t* r = value_new();
    dict_t* d = dict_new(&jsonDictTyep);
    value_set_map(r, d);
    return r;
}

/**
 * @brief 向JSON映射中添加值
 * @param root JSON映射对象指针
 * @param key 键名
 * @param v 要插入的值
 * @return 成功返回1，失败返回0
 */
int json_map_put_value(json_t* root, sds_t key, json_t* v) {
    if (!value_is_map(root)) return 0;
    dict_entry_t* node = dict_find(root->value.map_value, key);
    if (node != NULL) {
        // 如果键已存在，替换旧值
        json_t* old_v = (json_t*)dict_get_entry_val(node);
        value_delete(old_v);
        dict_set_val(root->value.map_value, node, v);
        return 1;
    } else {
        // 添加新的键值对
        return dict_add(root->value.map_value, key, v);
    }
}

/**
 * @brief 向JSON映射中添加字符串值
 * @param root JSON映射对象指针
 * @param key 键名
 * @param sv 字符串值
 * @return 成功返回1，失败返回0
 */
int json_map_put_sds(json_t* root, sds_t key, sds_t sv) {
    if (!value_is_map(root)) return 0;
    json_t* v = value_new();
    value_set_sds(v, sv);
    return json_map_put_value(root, key, v);
}

/**
 * @brief 向JSON映射中添加64位整数值
 * @param root JSON映射对象指针
 * @param key 键名
 * @param ll 64位整数值
 * @return 成功返回1，失败返回0
 */
int json_map_put_int64(json_t* root, sds_t key, int64_t ll) {
    if (!value_is_map(root)) return 0;
    json_t* v = value_new();
    value_set_int64(v, ll);
    return json_map_put_value(root, key, v);
}

/**
 * @brief 向JSON映射中添加布尔值
 * @param root JSON映射对象指针
 * @param key 键名
 * @param b 布尔值
 * @return 成功返回1，失败返回0
 */
int json_map_put_bool(json_t* root, sds_t key, bool b) {
    if (!value_is_map(root)) return 0;
    json_t* v = value_new();
    value_set_bool(v, b);
    return json_map_put_value(root, key, v);
}

/**
 * @brief 向JSON映射中添加double值
 * @param root JSON映射对象指针
 * @param key 键名
 * @param d double值
 * @return 成功返回1，失败返回0
 */
int json_map_put_double(json_t* root, sds_t key, double d) {
    if (!value_is_map(root)) return 0;
    json_t* v = value_new();
    value_set_double(v, d);
    return json_map_put_value(root, key, v);
}

/**
 * @brief 向JSON映射中添加null值
 * @param root JSON映射对象指针
 * @param key 键名
 * @return 成功返回1，失败返回0
 */
int json_map_put_null(json_t* root, sds_t key) {
    if (!value_is_map(root)) return 0;
    json_t* v = value_new();
    value_set_null(v);
    return json_map_put_value(root, key, v);
}

/**
 * @brief 从JSON映射中获取值
 * @param root JSON映射对象指针
 * @param key 键名
 * @return 返回对应的值指针，不存在返回NULL
 */
json_t* json_map_get(json_t* root, sds_t key) {
    if (!value_is_map(root)) return NULL;
    dict_entry_t* node = dict_find(root->value.map_value, key);
    if (node == NULL) return NULL;
    return (json_t*)dict_get_entry_val(node);
}

/**
 * @brief 从JSON映射中删除指定键
 * @param root JSON映射对象指针
 * @param key 要删除的键名
 * @return 成功返回1，失败返回0
 */
int json_map_delete(json_t* root, sds_t key) {
    if (!value_is_map(root)) return 0;
    dict_entry_t* node = dict_find(root->value.map_value, key);
    if (node == NULL) return 0;
    json_t* v = (json_t*)dict_get_entry_val(node);
    value_delete(v); // 释放值内存
    return dict_remove(root->value.map_value, key);
}

// array
/**
 * @brief 创建新的JSON数组对象
 * @return 返回初始化的JSON数组对象指针，失败返回NULL
 */
json_t* json_array_new() {
    json_t* r = value_new();
    list_t* l = list_new();
    value_set_list(r, l);
    return r;
}

/**
 * @brief 向JSON数组末尾追加值
 * @param root JSON数组对象指针
 * @param v 要追加的值
 * @return 成功返回1，失败返回0
 */
int json_array_append_value(json_t* root, json_t* v) {
    if (!value_is_list(root)) return 0;
    return list_append(root->value.list_value, v);
}

/**
 * @brief 向JSON数组末尾追加字符串值
 * @param root JSON数组对象指针
 * @param sv 字符串值
 * @return 成功返回1，失败返回0
 */
int json_array_append_sds(json_t* root, sds_t sv) {
    if (!value_is_list(root)) return 0;
    json_t* v = value_new();
    value_set_sds(v, sv);
    return json_array_append_value(root, v);
}

/**
 * @brief 向JSON数组末尾追加64位整数值
 * @param root JSON数组对象指针
 * @param ll 64位整数值
 * @return 成功返回1，失败返回0
 */
int json_array_append_int64(json_t* root, int64_t ll) {
    if (!value_is_list(root)) return 0;
    json_t* v = value_new();
    value_set_int64(v, ll);
    return json_array_append_value(root, v);
}

/**
 * @brief 向JSON数组末尾追加布尔值
 * @param root JSON数组对象指针
 * @param b 布尔值
 * @return 成功返回1，失败返回0
 */
int json_array_append_bool(json_t* root, bool b) {
    if (!value_is_list(root)) return 0;
    json_t* v = value_new();
    value_set_bool(v, b);
    return json_array_append_value(root, v);
}

/**
 * @brief 向JSON数组末尾追加double值
 * @param root JSON数组对象指针
 * @param d double值
 * @return 成功返回1，失败返回0
 */
int json_array_append_double(json_t* root, double d) {
    if (!value_is_list(root)) return 0;
    json_t* v = value_new();
    value_set_double(v, d);
    return json_array_append_value(root, v);
}

/**
 * @brief 向JSON数组末尾追加null值
 * @param root JSON数组对象指针
 * @return 成功返回1，失败返回0
 */
int json_array_append_null(json_t* root) {
    if (!value_is_list(root)) return 0;
    json_t* v = value_new();
    value_set_null(v);
    return json_array_append_value(root, v);
}

/**
 * @brief 获取JSON数组指定索引的值
 * @param root JSON数组对象指针
 * @param index 索引位置
 * @return 返回对应的值指针，索引越界返回NULL
 */
json_t* json_array_get(json_t* root, size_t index) {
    if (!value_is_list(root)) return NULL;
    return list_get(root->value.list_value, index);
}

/**
 * @brief 删除JSON数组指定索引的值
 * @param root JSON数组对象指针
 * @param index 要删除的索引位置
 * @return 成功返回1，失败返回0
 */
int json_array_remove(json_t* root, size_t index) {
    if (!value_is_list(root)) return 0;
    json_t* v = list_get(root->value.list_value, index);
    if (v == NULL) return 0;
    value_delete(v); // 释放值内存
    return list_remove(root->value.list_value, index);
}

/**
 * @brief 获取JSON数组的长度
 * @param root JSON数组对象指针
 * @return 返回数组长度，非数组类型返回0
 */
size_t json_array_size(json_t* root) {
    if (!value_is_list(root)) return 0;
    return list_size(root->value.list_value);
}

/**
 * @brief 在JSON数组指定位置插入值
 * @param root JSON数组对象指针
 * @param index 插入位置索引
 * @param value 要插入的值
 * @return 成功返回1，失败返回0
 */
int json_array_insert_value(json_t* root, size_t index, json_t* value) {
    if (!value_is_list(root)) return 0;
    return list_insert(root->value.list_value, index, value);
}

/**
 * @brief 获取JSON映射的大小（键值对数量）
 * @param root JSON映射对象指针
 * @return 返回映射中键值对数量，非映射类型返回0
 */
size_t json_map_size(json_t* root) {
    if (!value_is_map(root)) return 0;
    return dict_size(root->value.map_value);
}