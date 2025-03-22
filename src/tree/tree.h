#ifndef __LATTE_TREE_H
#define __LATTE_TREE_H

#include "iterator/iterator.h"
#include "cmp/cmp.h"

typedef struct tree_node_t {

} tree_node_t;

typedef struct tree_t {
    int add(tree_t* tree, void* key, void* value);
    int cmp_key(tree_t* tree, void* key, void* key2);
    int remove(tree_t* tree, void* key);
    void* get(tree_t* tree, void* key);
} tree_t;

#endif