/**
 * @file tree.h
 * @brief 通用树接口定义
 *        定义树节点（tree_node_t）和树操作虚函数表（tree_t），
 *        具体实现由 AVL 树、B+ 树、红黑树等子模块提供。
 */
#ifndef __LATTE_TREE_H
#define __LATTE_TREE_H

#include "iterator/iterator.h"
#include "cmp/cmp.h"

/**
 * @brief 通用树节点结构体（占位，当前无字段）
 */
typedef struct tree_node_t {

} tree_node_t;

/**
 * @brief 通用树接口结构体（函数指针虚函数表）
 *
 * 定义树的标准操作接口，具体实现由 AVL 树、B+ 树等子类提供。
 */
typedef struct tree_t {
    int add(tree_t* tree, void* key, void* value);   /**< 添加键值对 */
    int cmp_key(tree_t* tree, void* key, void* key2); /**< 比较两个键 */
    int remove(tree_t* tree, void* key);             /**< 删除指定键 */
    void* get(tree_t* tree, void* key);              /**< 按键查找值 */
} tree_t;

#endif
