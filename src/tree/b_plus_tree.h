#ifndef __LATTE_B_PLUS_TREE_H
#define __LATTE_B_PLUS_TREE_H

#include "iterator/iterator.h"
#include "cmp/cmp.h"
#include "vector/vector.h"
/**
 * B+树的特点
    1. 所有叶子节点都在同一层。
    2. 非叶子节点可以包含指向子节点的指针和关键字。
    3. 叶子节点包含关键字和相应的数据记录地址或数据本身。
    4. 非叶子节点用于引导搜索。
    5. 每个节点最多包含M个子节点（M是树的阶）。
    6. 每个节点至少包含ceil(M/2)个子节点（除了根节点）。
 */

typedef struct b_plus_tree_node_t {
    int isLeaf;
    int numKeys;
    void** keys; // 存储关键字  void* keys[order - 1] 
    void** data; // 存储数据记录或指向数据记录的指针 void* data[order - 1] 
    struct b_plus_tree_node_t **children; // 存储子节点指针 void* children[order] 
    struct b_plus_tree_node_t* next;
} b_plus_tree_node_t;
b_plus_tree_node_t* b_plus_tree_node_new(int order, int isLeaf);
void b_plus_tree_node_delete(b_plus_tree_node_t* node);

// typedef int (*cmp_func_t)(void*, void*);
typedef struct b_plus_tree_func_t {
    cmp_func* cmp;
    void (*free_node)(void* value);
} b_plus_tree_func_t;

typedef struct b_plus_tree_t {
    b_plus_tree_func_t* type;
    b_plus_tree_node_t* root;
    int order;
} b_plus_tree_t;
b_plus_tree_t* b_plus_tree_new(b_plus_tree_func_t* type, int order);
void b_plus_tree_insert(b_plus_tree_t* tree, void* key, void* data);
void* b_plus_tree_find(b_plus_tree_t* tree, void* key);
void b_plus_tree_update(b_plus_tree_t* tree, void* key, void* data);
typedef struct tree_node_info_t {
    b_plus_tree_node_t* parent;
    int index;
    b_plus_tree_node_t* leaf;
} tree_node_info_t;
bool b_plus_tree_remove(b_plus_tree_t* tree, void* key);
void b_plus_tree_delete(b_plus_tree_t* tree);
void b_plus_tree_insert_or_update(b_plus_tree_t* tree, void* key, void* data);

typedef struct b_plus_tree_iterator_data {
    b_plus_tree_node_t* node;
    int index;
    latte_pair_t pair;
} b_plus_tree_iterator_data;
latte_iterator_t* b_plus_tree_get_iterator(b_plus_tree_t* tree);

#endif