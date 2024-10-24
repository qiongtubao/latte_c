#ifndef __LATTE_AVL_SET_H
#define __LATTE_AVL_SET_H
#include "stdlib.h"
#include "set.h"
#include "tree/avlTree.h"

typedef struct avlTree avl_set_t;
#define  avlSetCreate avlTreeCreate

int avl_set_add(avl_set_t* set, void* key);
#define  avl_set_remove avlTreeRemove
#define avl_set_delete avlTreeRelease
#define  avl_set_size avlTreeSize
#define avl_set_get_iterator avlTreeGetIterator
int avl_set_contains(avl_set_t* set, void* element);

#define avl_set_func_t avlTreeType
extern avl_set_func_t avl_set_sds_func;
extern set_func_t avl_set_func;
set_t* avl_set_new(avl_set_func_t* type);


#endif



