#ifndef __LATTE_AVL_SET_H
#define __LATTE_AVL_SET_H
#include "stdlib.h"
#include "set.h"
#include "tree/avl_tree.h"

/**
 * @brief 基于AVL树的Set类型，底层直接就是avl_tree_t
 */
typedef struct avl_tree_t avl_set_t;
#define  avlSetCreate avl_tree_new

int avl_set_add(avl_set_t* set, void* key);
int avl_set_contains(avl_set_t* set, void* element);

/**
 * @brief AVLSet的操作函数表类型定义，复用AVL树的类型
 */
#define avl_set_func_t avl_tree_type_t
/**
 * @brief 键为sds字符串的AVLSet操作函数表实例
 */
extern avl_set_func_t avl_set_sds_func;
/**
 * @brief AVLSet的Set接口虚拟函数表
 */
extern set_func_t avl_set_func;

/**
 * @brief 创建一个新的基于AVL树实现的Set
 * @param type AVL树的具体类型操作函数表
 * @return set_t* 返回抽象的Set对象
 */
set_t* avl_set_new(avl_set_func_t* type);


#endif



