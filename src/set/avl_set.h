#ifndef __LATTE_AVL_SET_H
#define __LATTE_AVL_SET_H
#include "stdlib.h"
#include "set.h"
#include "tree/avlTree.h"

typedef struct avlTree avl_set_t;
#define  avlSetCreate avl_tree_new

int avl_set_add(avl_set_t* set, void* key);
#define  avl_set_remove avl_tree_remove
#define avl_set_delete avl_tree_delete
#define  avl_set_size avl_tree_size
#define avl_set_get_iterator avl_tree_get_iterator
int avl_set_contains(avl_set_t* set, void* element);

#define avl_set_func_t avl_tree_type_t
extern avl_set_func_t avl_set_sds_func;
extern set_func_t avl_set_func;
set_t* avl_set_new(avl_set_func_t* type);


#endif



