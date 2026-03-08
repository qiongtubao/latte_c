/*
 * avl_set.h - set 模块头文件
 * 
 * Latte C 库组件
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

#ifndef __LATTE_AVL_SET_H
#define __LATTE_AVL_SET_H
#include "stdlib.h"
#include "set.h"
#include "tree/avl_tree.h"

typedef struct avl_tree_t avl_set_t;
#define  avlSetCreate avl_tree_new

int avl_set_add(avl_set_t* set, void* key);
int avl_set_contains(avl_set_t* set, void* element);

#define avl_set_func_t avl_tree_type_t
extern avl_set_func_t avl_set_sds_func;
extern set_func_t avl_set_func;
set_t* avl_set_new(avl_set_func_t* type);


#endif



