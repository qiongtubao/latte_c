/*
 * tree.h - 树数据结构头文件
 * 
 * Latte C 库数据结构组件：通用树操作接口
 * 
 * 核心功能：
 * 1. 通用树接口定义
 * 2. 支持多种树类型 (红黑树、B+ 树、跳表等)
 * 3. 统一遍历接口
 * 4. 内存管理回调
 * 
 * 子模块：
 * - rb_tree.h: 红黑树实现
 * - maple_tree.h: 基于 maple 树的实现 (Linux 内核算法)
 * - skip_list.h: 跳表实现
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

#ifndef __LATTE_TREE_H
#define __LATTE_TREE_H

#include <stddef.h>
#include <stdint.h>

/* 树节点比较结果 */
#define TREE_LT (-1)  /* 小于 */
#define TREE_EQ 0      /* 等于 */
#define TREE_GT 1      /* 大于 */

/* 树遍历方向 */
typedef enum {
    TREE_PREORDER,     /* 前序遍历 */
    TREE_INORDER,      /* 中序遍历 */
    TREE_POSTORDER,    /* 后序遍历 */
    TREE_LEVELORDER    /* 层序遍历 */
} tree_order_t;

/* 遍历回调函数 */
typedef int (*tree_visit_fn)(void *key, void *value, void *user_data);

/* 树节点比较函数 */
typedef int (*tree_compare_fn)(const void *key1, const void *key2);

/* 树内存管理回调 */
typedef void (*tree_free_fn)(void *key, void *value);

/* 通用树结构 (前向声明) */
typedef struct tree_t tree_t;

/* 树配置结构 */
typedef struct tree_config_t {
    tree_compare_fn compare;   /* 比较函数 */
    tree_free_fn free_key;     /* 键释放函数 (可选) */
    tree_free_fn free_value;   /* 值释放函数 (可选) */
    void *user_data;           /* 用户自定义数据 */
} tree_config_t;

/* 创建树
 * 参数：config - 配置结构
 * 返回：成功返回树指针，失败返回 NULL
 */
tree_t* tree_new(tree_config_t *config);

/* 销毁树
 * 参数：tree - 待销毁的树
 */
void tree_delete(tree_t *tree);

/* 插入节点
 * 参数：tree - 树指针
 *       key - 键
 *       value - 值
 * 返回：成功返回 0，键已存在返回 1，失败返回 -1
 */
int tree_insert(tree_t *tree, void *key, void *value);

/* 删除节点
 * 参数：tree - 树指针
 *       key - 键
 * 返回：成功返回 0，未找到返回 1，失败返回 -1
 */
int tree_remove(tree_t *tree, void *key);

/* 查找节点
 * 参数：tree - 树指针
 *       key - 键
 * 返回：找到返回值指针，未找到返回 NULL
 */
void* tree_find(tree_t *tree, void *key);

/* 获取最小键
 * 参数：tree - 树指针
 * 返回：最小键指针，空树返回 NULL
 */
void* tree_min(tree_t *tree);

/* 获取最大键
 * 参数：tree - 树指针
 * 返回：最大键指针，空树返回 NULL
 */
void* tree_max(tree_t *tree);

/* 获取树大小
 * 参数：tree - 树指针
 * 返回：节点数量
 */
size_t tree_size(tree_t *tree);

/* 遍历树
 * 参数：tree - 树指针
 *       order - 遍历顺序
 *       visit_fn - 访问回调
 *       user_data - 用户数据
 * 返回：成功返回 0，遍历中断返回 1
 */
int tree_traverse(tree_t *tree, tree_order_t order, tree_visit_fn visit_fn, void *user_data);

/* 获取树高度
 * 参数：tree - 树指针
 * 返回：树高度 (空树返回 0)
 */
int tree_height(tree_t *tree);

#endif /* __LATTE_TREE_H */
