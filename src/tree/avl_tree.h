
#ifndef __LATTE_AVL_TREE_H
#define __LATTE_AVL_TREE_H
#include "stdlib.h"
#include "iterator/iterator.h"
#include "stdbool.h"

/**
 * @brief AVL 自平衡二叉搜索树实现
 *
 * 通过 avl_tree_type_t 虚函数表支持泛型键值对，
 * 所有操作均保持树的高度平衡（|左高-右高| <= 1）。
 */

/** @brief AVL 节点前向声明 */
typedef struct avl_node_t avl_node_t;

/**
 * @brief 获取节点关联值的函数类型
 * @param node 节点指针（const void*）
 * @return void* 节点的值指针
 */
typedef void* (*get_value_func)(const void*);

/**
 * @brief AVL 树操作虚函数表
 * 通过函数指针实现键值对类型无关的通用操作。
 */
typedef struct avl_tree_type_t {
    int (*operator)(void* f1, void* f2);            /**< 键比较函数：负/0/正 对应 f1<f2/f1==f2/f1>f2 */
    void (*release_node)(avl_node_t* node);         /**< 节点释放函数（含键值内存） */
    avl_node_t* (*create_node)(void* key, void* value); /**< 节点创建工厂函数 */
    get_value_func get_value;                        /**< 从节点获取值的函数 */
    void (*node_set_val)(avl_node_t* node, void* value); /**< 更新节点值的函数 */
} avl_tree_type_t;

/**
 * @brief AVL 树节点结构体
 * 不直接存储 value，由 type->get_value 从 key 派生或通过子类扩展存储。
 */
typedef struct avl_node_t {
    void* key;                  /**< 节点键指针 */
    struct avl_node_t* left;    /**< 左子节点 */
    struct avl_node_t* right;   /**< 右子节点 */
    int height;                 /**< 节点高度（叶节点为 1） */
} avl_node_t;

/**
 * @brief AVL 树中序遍历迭代器（内部状态，通过显式栈模拟递归）
 */
typedef struct avl_tree_iterator_t {
    avl_node_t* current;    /**< 当前正在处理的节点 */
    avl_node_t** stack;     /**< 显式栈（存储待访问节点指针） */
    size_t stackSize;       /**< 栈的总容量 */
    size_t top;             /**< 栈顶指针（已使用元素数） */
} avl_tree_iterator_t;

/**
 * @brief AVL 树结构体
 */
typedef struct avl_tree_t {
    avl_tree_type_t* type;  /**< 虚函数表指针（键值操作） */
    avl_node_t* root;       /**< 根节点指针，空树为 NULL */
} avl_tree_t;

/**
 * @brief 创建一个新的 AVL 树
 * @param type 虚函数表指针（必须非 NULL）
 * @return avl_tree_t* 新建树指针
 */
avl_tree_t* avl_tree_new(avl_tree_type_t* type);

/**
 * @brief 释放 AVL 树及其所有节点（调用 type->release_node）
 * @param tree 目标树指针
 */
void avl_tree_delete(avl_tree_t* tree);

/**
 * @brief 插入或更新键值对（若键已存在则调用 node_set_val 更新值）
 * @param tree  目标树指针
 * @param key   键指针
 * @param value 值指针
 * @return int 成功返回 1，失败返回 0
 */
int avl_tree_put(avl_tree_t* tree, void* key, void* value);

/**
 * @brief 按键查找并返回节点的值（通过 type->get_value）
 * @param tree 目标树指针
 * @param key  键指针
 * @return void* 找到返回值指针，未找到返回 NULL
 */
void* avl_tree_get(avl_tree_t* tree, void* key);

/**
 * @brief 获取树中最小键的节点
 * @param tree 目标树指针
 * @return avl_node_t* 最小节点指针，空树返回 NULL
 */
avl_node_t* avl_tree_get_min(avl_tree_t* tree);

/**
 * @brief 按键删除节点
 * @param tree 目标树指针
 * @param key  要删除的键指针
 * @return int 成功删除返回 1，未找到返回 0
 */
int avl_tree_remove(avl_tree_t* tree, void* key);

/**
 * @brief 获取树中节点总数
 * @param tree 目标树指针
 * @return int 节点数量
 */
int avl_tree_size(avl_tree_t* tree);

/**
 * @brief 清空树（删除所有节点，树结构保留）
 * @param tree 目标树指针
 */
void avl_tree_clear(avl_tree_t* tree);

/**
 * @brief 兼容 latte_iterator_t 接口的 AVL 树中序迭代器
 * 嵌入 latte_iterator_t 和 latte_pair_t，通过类型转换兼容通用迭代器接口。
 */
typedef struct avl_tree_latte_iterator_t {
    latte_iterator_t iterator;  /**< 通用迭代器基类（必须为第一个字段） */
    latte_pair_t pair;          /**< 当前迭代到的键值对 */
    avl_tree_t* tree;           /**< 所属 AVL 树指针 */
} avl_tree_latte_iterator_t;

/**
 * @brief 获取 AVL 树的中序迭代器（升序遍历）
 * @param tree 目标树指针
 * @return latte_iterator_t* 新建迭代器（调用方负责释放）
 */
latte_iterator_t* avl_tree_get_iterator(avl_tree_t* tree);



#endif
