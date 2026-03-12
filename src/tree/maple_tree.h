#ifndef __LATTE_MAPLE_TREE_H
#define __LATTE_MAPLE_TREE_H

#include <stdlib.h>
#include <stdbool.h>
#include "iterator/iterator.h"

/**
 * @brief Maple 树节点结构体（基于 Treap 实现的随机平衡二叉搜索树）
 *
 * 每个节点存储键值对及随机优先级，通过优先级维持 Treap 的堆性质，
 * 结合 BST 性质实现 O(log N) 期望时间复杂度的插入/删除/查找。
 */
typedef struct maple_node_t {
    void* key;                      /**< 节点键指针 */
    void* value;                    /**< 节点值指针 */
    struct maple_node_t *left;      /**< 左子节点 */
    struct maple_node_t *right;     /**< 右子节点 */
    int priority;                   /**< Treap 随机优先级（堆性质：父 >= 子） */
} maple_node_t;

/**
 * @brief 从节点获取值的函数类型（与 avl 接口保持一致）
 * @param node 节点指针（const void*）
 * @return void* 节点的值指针
 */
typedef void* (*maple_get_value_func)(const void*);

/**
 * @brief Maple 树操作函数表（支持泛型键值）
 */
typedef struct maple_tree_type_t {
    int (*compare)(void* k1, void* k2); /**< 键比较函数：k1>k2 返回正，k1<k2 返回负，相等返回 0 */
    void (*release_key)(void* key);     /**< 释放键内存的函数 */
    void (*release_value)(void* value); /**< 释放值内存的函数 */
} maple_tree_type_t;

/**
 * @brief Maple 树结构体
 */
typedef struct maple_tree_t {
    maple_node_t *root;         /**< 根节点指针，空树为 NULL */
    maple_tree_type_t *type;    /**< 操作函数表指针 */
    size_t size;                /**< 当前节点数量 */
} maple_tree_t;

/**
 * @brief 创建一个新的 Maple 树
 * @param type 操作函数表指针（必须非 NULL）
 * @return maple_tree_t* 新建树指针
 */
maple_tree_t* maple_tree_new(maple_tree_type_t *type);

/**
 * @brief 释放 Maple 树及其所有节点
 * @param tree 目标树指针
 */
void maple_tree_delete(maple_tree_t *tree);

/**
 * @brief 插入或更新键值对
 * @param tree  目标树指针
 * @param key   键指针
 * @param value 值指针
 * @return int 新增返回 1，更新返回 0，失败返回 -1
 */
int maple_tree_put(maple_tree_t *tree, void *key, void *value);

/**
 * @brief 按键查找值
 * @param tree 目标树指针
 * @param key  键指针
 * @return void* 找到返回值指针，未找到返回 NULL
 */
void* maple_tree_get(maple_tree_t *tree, void *key);

/**
 * @brief 按键删除节点
 * @param tree 目标树指针
 * @param key  要删除的键指针
 * @return int 成功删除返回 1，未找到返回 0
 */
int maple_tree_remove(maple_tree_t *tree, void *key);

/**
 * @brief 获取树中节点总数
 * @param tree 目标树指针
 * @return size_t 节点数量
 */
size_t maple_tree_size(maple_tree_t *tree);

/**
 * @brief 清空树中所有节点（树结构体保留）
 * @param tree 目标树指针
 */
void maple_tree_clear(maple_tree_t *tree);

/**
 * @brief 获取 Maple 树的中序迭代器（升序遍历）
 * @param tree 目标树指针
 * @return latte_iterator_t* 新建迭代器（调用方负责释放）
 */
latte_iterator_t* maple_tree_get_iterator(maple_tree_t *tree);

#endif
