
#ifndef __LATTE_AVL_TREE_H
#define __LATTE_AVL_TREE_H
#include "stdlib.h"
#include "iterator/iterator.h"
#include "stdbool.h"
/**
 * avl 
 */
typedef struct avl_node_t avl_node_t;
typedef void* (*get_value_func)(const void*);
typedef struct avl_tree_type_t {
    int (*operator)(void* f1, void* f2);
    void (*release_node)(avl_node_t* node);
    avl_node_t* (*create_node)(void* key, void* value);
    get_value_func get_value;
    void (*node_set_val)(avl_node_t* node, void* value);
} avl_tree_type_t;

typedef struct avl_node_t {
    void* key;
    struct avl_node_t* left;
    struct avl_node_t* right;
    int height;
    // void* value; // 值可以是任意类型，使用 void* 通用指针
} avl_node_t;

typedef struct avl_tree_iterator_t {
    avl_node_t* current;
    avl_node_t** stack;
    size_t stackSize;
    size_t top;
} avl_tree_iterator_t;

typedef struct avl_tree_t {
    avl_tree_type_t* type;
    avl_node_t* root;
} avl_tree_t;

avl_tree_t* avl_tree_new(avl_tree_type_t* type);
void avl_tree_delete(avl_tree_t* tree);

int avl_tree_put(avl_tree_t* tree, void* key, void* value);
void* avl_tree_get(avl_tree_t* tree, void* key);
avl_node_t* avl_tree_get_min(avl_tree_t* tree);
int avl_tree_remove(avl_tree_t* tree, void* key);
void avl_tree_delete(avl_tree_t* tree);
int avl_tree_size(avl_tree_t* tree);


// avl_tree_iterator_t* avl_tree_iterator_new(avl_tree_t* tree);
// bool avl_tree_iterator_has_next(avl_tree_iterator_t* iterator);
// avl_node_t* avl_tree_iterator_tNext(avl_tree_iterator_t* iterator);
// void avl_tree_iterator_tRelease(avl_tree_iterator_t* iterator);

typedef struct avl_tree_latte_iterator_t {
    latte_iterator_t iterator;
    latte_pair_t pair;
    avl_tree_t* tree;
} avl_tree_latte_iterator_t;

latte_iterator_t* avl_tree_get_iterator(avl_tree_t* tree);



#endif