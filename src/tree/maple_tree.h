#ifndef __LATTE_MAPLE_TREE_H
#define __LATTE_MAPLE_TREE_H

#include <stdlib.h>
#include <stdbool.h>
#include "iterator/iterator.h"

// Define the Maple Tree Node
typedef struct maple_node_t {
    void* key;
    void* value;
    struct maple_node_t *left;
    struct maple_node_t *right;
    int priority; // For Treap balancing
} maple_node_t;

// Define the function pointer type for getting a value (if needed, following avl pattern)
typedef void* (*maple_get_value_func)(const void*);

// Define the Maple Tree Type (configuration)
typedef struct maple_tree_type_t {
    int (*compare)(void* k1, void* k2); // Returns 1 if k1 > k2, -1 if k1 < k2, 0 if equal
    void (*release_key)(void* key);
    void (*release_value)(void* value);
} maple_tree_type_t;

// Define the Maple Tree structure
typedef struct maple_tree_t {
    maple_node_t *root;
    maple_tree_type_t *type;
    size_t size;
} maple_tree_t;

// API Functions
maple_tree_t* maple_tree_new(maple_tree_type_t *type);
void maple_tree_delete(maple_tree_t *tree);

int maple_tree_put(maple_tree_t *tree, void *key, void *value);
void* maple_tree_get(maple_tree_t *tree, void *key);
int maple_tree_remove(maple_tree_t *tree, void *key);
size_t maple_tree_size(maple_tree_t *tree);
void maple_tree_clear(maple_tree_t *tree);

// Iterator
latte_iterator_t* maple_tree_get_iterator(maple_tree_t *tree);

#endif
