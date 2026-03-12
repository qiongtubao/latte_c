


#ifndef __LATTE_SET_H
#define __LATTE_SET_H

#include "dict/dict.h"
#include "dict/dict_plugins.h"
#include <stdbool.h>
#include "iterator/iterator.h"
#include "zmalloc/zmalloc.h"



typedef struct set_t set_t;

/**
 * @brief Set 操作函数表定义，实现多态(Polymorphism)的接口
 * 允许使用不同底层数据结构(如哈希表、AVL树等)来实现Set
 */
// typedef struct setNode setNode;
typedef struct set_func_t {
    // char* (*getName)();
    int (*add)(set_t* set, void* element);      /**< 添加元素到集合中 */
    int (*remove)(set_t* set, void* element);   /**< 从集合中删除元素 */
    int (*contains)(set_t* set, void* element); /**< 检查集合是否包含元素 */
    size_t (*size)(set_t* set);                 /**< 获取集合中的元素数量 */
    void (*release)(set_t* set);                /**< 释放集合及其底层数据结构 */
    void (*clear)(set_t* set);                  /**< 清空集合中的所有元素 */
    latte_iterator_t* (*getIterator)(set_t* set);/**< 获取集合的迭代器 */
} set_func_t;
//主要是用于不同set类型 如果无法明确用途就使用动态方式，
//如果确定的话最好就用指定类型
/**
 * @brief Set数据结构抽象层
 */
typedef struct set_t {
    set_func_t* type;   /**< 函数指针表，指向具体的Set实现（例如hash_set, avl_set） */
    void* data;         /**< 底层实际的数据结构指针（例如指向哈希表或AVL树结构） */
} set_t;

// typedef struct setIteratorType {
//     bool  (*hasNext)(setIterator* iterator);
//     void* (*next)(setIterator* iterator);
//     void (*release)(setIterator* iterator);
// } setIteratorType;
// typedef struct setIterator {
//     setIteratorType* type;
//     void* data;
// } setIterator;

// typedef struct setKeyType {

// } setKeyType;

/**
 * @brief 创建一个新的Set对象
 * @param type Set的操作函数表
 * @param data Set的底层数据结构实例
 * @return set_t* 返回新创建的Set对象，如果失败返回NULL
 */
set_t* set_new(set_func_t* type, void* data);

/**
 * @brief 向Set中添加一个元素
 * @param s Set对象
 * @param key 要添加的元素指针
 * @return int 状态码：-1 表示失败，0 表示元素已存在，1 表示新添加成功
 */
// -1 失败  0 已经存在  1 新添加
int set_add(set_t* s, void* key);

/**
 * @brief 检查Set中是否包含指定的元素
 * @param s Set对象
 * @param key 要检查的元素指针
 * @return int 状态码：0 表示不存在，1 表示存在
 */
//0 不存在 1 存在
int set_contains(set_t* s, void* key);

/**
 * @brief 从Set中移除指定的元素
 * @param s Set对象
 * @param key 要移除的元素指针
 * @return int 移除是否成功
 */
int set_remove(set_t* s, void* key);

/**
 * @brief 获取Set中元素的数量
 * @param s Set对象
 * @return size_t 元素数量
 */
size_t set_size(set_t* s);

/**
 * @brief 释放Set对象及其底层结构
 * @param s Set对象
 */
void set_delete(set_t* s);

/**
 * @brief 清空Set中的所有元素，但不释放Set对象本身
 * @param s Set对象
 */
void set_clear(set_t* s);

/**
 * @brief 获取Set的迭代器，用于遍历Set中的元素
 * @param s Set对象
 * @return latte_iterator_t* 返回创建的迭代器
 */
latte_iterator_t* set_get_iterator(set_t* s);

// in avlSet.h
// set_t* avl_set_new(avl_tree_type_t* type);
// in hash_set.h
//set_t* hash_set_new(hash_set_func_t* type);
#endif