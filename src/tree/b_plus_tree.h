#ifndef __LATTE_B_PLUS_TREE_H
#define __LATTE_B_PLUS_TREE_H

#include "iterator/iterator.h"
#include "cmp/cmp.h"
#include "vector/vector.h"

/**
 * @brief B+ 树数据结构
 *
 * B+ 树特点：
 *   1. 所有叶子节点都在同一层。
 *   2. 非叶子节点存储关键字和子节点指针，仅用于引导查找。
 *   3. 叶子节点存储关键字及对应的数据记录（或指针）。
 *   4. 每个节点最多含 M 个子节点（M 为树的阶）。
 *   5. 每个非根节点至少含 ceil(M/2) 个子节点。
 *   6. 叶子节点通过 next 指针串联，支持顺序遍历。
 */

/**
 * @brief B+ 树节点结构体
 */
typedef struct b_plus_tree_node_t {
    int isLeaf;                             /**< 是否为叶子节点（1=叶子，0=内部节点） */
    int numKeys;                            /**< 当前存储的关键字数量 */
    void** keys;                            /**< 关键字数组（容量 order-1） */
    void** data;                            /**< 数据记录数组（容量 order-1，仅叶子节点有效） */
    struct b_plus_tree_node_t **children;   /**< 子节点指针数组（容量 order，仅内部节点有效） */
    struct b_plus_tree_node_t* next;        /**< 下一个叶子节点指针（用于顺序遍历，仅叶子节点有效） */
} b_plus_tree_node_t;

/**
 * @brief 创建一个新的 B+ 树节点
 * @param order  树的阶（决定数组容量）
 * @param isLeaf 是否为叶子节点（1=叶子）
 * @return b_plus_tree_node_t* 新建节点指针
 */
b_plus_tree_node_t* b_plus_tree_node_new(int order, int isLeaf);

/**
 * @brief 释放一个 B+ 树节点（不递归释放子节点）
 * @param node 目标节点指针
 */
void b_plus_tree_node_delete(b_plus_tree_node_t* node);

/**
 * @brief B+ 树操作函数表（支持泛型键值）
 */
typedef struct b_plus_tree_func_t {
    cmp_func* cmp;              /**< 键比较函数：返回负/0/正 对应 a<b/a==b/a>b */
    void (*free_node)(void* value); /**< 释放数据记录的函数 */
} b_plus_tree_func_t;

/**
 * @brief B+ 树结构体
 */
typedef struct b_plus_tree_t {
    b_plus_tree_func_t* type;   /**< 操作函数表指针 */
    b_plus_tree_node_t* root;   /**< 根节点指针，空树为 NULL */
    int order;                  /**< 树的阶（每个节点最多 order 个子节点） */
} b_plus_tree_t;

/**
 * @brief 创建一个新的 B+ 树
 * @param type  操作函数表指针（必须非 NULL）
 * @param order 树的阶（>= 2）
 * @return b_plus_tree_t* 新建树指针
 */
b_plus_tree_t* b_plus_tree_new(b_plus_tree_func_t* type, int order);

/**
 * @brief 向 B+ 树插入键值对（键不存在时插入）
 * @param tree 目标树指针
 * @param key  键指针
 * @param data 数据记录指针
 */
void b_plus_tree_insert(b_plus_tree_t* tree, void* key, void* data);

/**
 * @brief 按键查找数据记录
 * @param tree 目标树指针
 * @param key  键指针
 * @return void* 找到返回数据指针，未找到返回 NULL
 */
void* b_plus_tree_find(b_plus_tree_t* tree, void* key);

/**
 * @brief 更新指定键对应的数据记录
 * @param tree 目标树指针
 * @param key  键指针
 * @param data 新数据记录指针
 */
void b_plus_tree_update(b_plus_tree_t* tree, void* key, void* data);

/**
 * @brief 查找时的辅助信息（定位叶子节点及其父节点）
 */
typedef struct tree_node_info_t {
    b_plus_tree_node_t* parent; /**< 目标节点的父节点 */
    int index;                  /**< 目标节点在父节点 children 数组中的索引 */
    b_plus_tree_node_t* leaf;   /**< 找到的叶子节点 */
} tree_node_info_t;

/**
 * @brief 按键删除节点
 * @param tree 目标树指针
 * @param key  要删除的键指针
 * @return bool 找到并删除返回 true，未找到返回 false
 */
bool b_plus_tree_remove(b_plus_tree_t* tree, void* key);

/**
 * @brief 释放整个 B+ 树及所有节点
 * @param tree 目标树指针
 */
void b_plus_tree_delete(b_plus_tree_t* tree);

/**
 * @brief 插入或更新键值对（键存在则更新，不存在则插入）
 * @param tree 目标树指针
 * @param key  键指针
 * @param data 数据记录指针
 */
void b_plus_tree_insert_or_update(b_plus_tree_t* tree, void* key, void* data);

/**
 * @brief B+ 树迭代器内部数据（顺序遍历叶子节点）
 */
typedef struct b_plus_tree_iterator_data {
    b_plus_tree_node_t* node;   /**< 当前叶子节点指针 */
    int index;                  /**< 当前节点内的关键字索引 */
    latte_pair_t pair;          /**< 当前迭代到的键值对 */
} b_plus_tree_iterator_data;

/**
 * @brief 获取 B+ 树的顺序迭代器（从最小键开始遍历）
 * @param tree 目标树指针
 * @return latte_iterator_t* 新建迭代器（调用方负责释放）
 */
latte_iterator_t* b_plus_tree_get_iterator(b_plus_tree_t* tree);

#endif
