#include "maple_tree.h"
#include "zmalloc/zmalloc.h"
#include <stdlib.h>
#include <stdio.h>

/* --- Helper Functions --- */

static maple_node_t* create_node(void *key, void *value) {
    maple_node_t *node = zmalloc(sizeof(maple_node_t));
    if (!node) return NULL;
    node->key = key;
    node->value = value;
    node->left = NULL;
    node->right = NULL;
    node->priority = rand();
    return node;
}

static void release_node_recursive(maple_tree_t *tree, maple_node_t *node) {
    if (!node) return;
    release_node_recursive(tree, node->left);
    release_node_recursive(tree, node->right);
    
    if (tree->type->release_key) {
        tree->type->release_key(node->key);
    }
    if (tree->type->release_value) {
        tree->type->release_value(node->value);
    }
    zfree(node);
}

/* --- Rotations --- */

static maple_node_t* rotate_right(maple_node_t *y) {
    maple_node_t *x = y->left;
    maple_node_t *T2 = x->right;
    
    x->right = y;
    y->left = T2;
    
    return x;
}

static maple_node_t* rotate_left(maple_node_t *x) {
    maple_node_t *y = x->right;
    maple_node_t *T2 = y->left;
    
    y->left = x;
    x->right = T2;
    
    return y;
}

/* --- Core Logic --- */

static maple_node_t* insert_node(maple_tree_t *tree, maple_node_t *node, void *key, void *value, int *added) {
    if (!node) {
        *added = 1;
        tree->size++;
        return create_node(key, value);
    }
    
    int cmp = tree->type->compare(key, node->key);
    
    if (cmp < 0) {
        node->left = insert_node(tree, node->left, key, value, added);
        if (node->left->priority > node->priority) {
            node = rotate_right(node);
        }
    } else if (cmp > 0) {
        node->right = insert_node(tree, node->right, key, value, added);
        if (node->right->priority > node->priority) {
            node = rotate_left(node);
        }
    } else {
        // Key exists, update value
        if (tree->type->release_value) {
            tree->type->release_value(node->value);
        }
        node->value = value;
        *added = 0;
    }
    return node;
}

static maple_node_t* remove_node(maple_tree_t *tree, maple_node_t *node, void *key, int *removed) {
    if (!node) {
        *removed = 0;
        return NULL;
    }

    int cmp = tree->type->compare(key, node->key);

    if (cmp < 0) {
        node->left = remove_node(tree, node->left, key, removed);
    } else if (cmp > 0) {
        node->right = remove_node(tree, node->right, key, removed);
    } else {
        // Found node to remove
        *removed = 1;
        if (!node->left) {
            maple_node_t *temp = node->right;
            if (tree->type->release_key) tree->type->release_key(node->key);
            if (tree->type->release_value) tree->type->release_value(node->value);
            zfree(node);
            tree->size--;
            return temp;
        } else if (!node->right) {
            maple_node_t *temp = node->left;
            if (tree->type->release_key) tree->type->release_key(node->key);
            if (tree->type->release_value) tree->type->release_value(node->value);
            zfree(node);
            tree->size--;
            return temp;
        }

        // Two children: Rotate down according to priority
        if (node->left->priority > node->right->priority) {
            node = rotate_right(node);
            node->right = remove_node(tree, node->right, key, removed);
        } else {
            node = rotate_left(node);
            node->left = remove_node(tree, node->left, key, removed);
        }
    }
    return node;
}

/* --- API Implementation --- */

maple_tree_t* maple_tree_new(maple_tree_type_t *type) {
    if (!type || !type->compare) return NULL;
    
    maple_tree_t *tree = zmalloc(sizeof(maple_tree_t));
    if (!tree) return NULL;
    
    tree->root = NULL;
    tree->type = type;
    tree->size = 0;
    return tree;
}

void maple_tree_delete(maple_tree_t *tree) {
    if (!tree) return;
    maple_tree_clear(tree);
    zfree(tree);
}

void maple_tree_clear(maple_tree_t *tree) {
    if (!tree) return;
    release_node_recursive(tree, tree->root);
    tree->root = NULL;
    tree->size = 0;
}

int maple_tree_put(maple_tree_t *tree, void *key, void *value) {
    if (!tree) return 0;
    int added = 0;
    tree->root = insert_node(tree, tree->root, key, value, &added);
    return added;
}

void* maple_tree_get(maple_tree_t *tree, void *key) {
    if (!tree || !tree->root) return NULL;
    
    maple_node_t *curr = tree->root;
    while (curr) {
        int cmp = tree->type->compare(key, curr->key);
        if (cmp == 0) return curr->value;
        if (cmp < 0) curr = curr->left;
        else curr = curr->right;
    }
    return NULL;
}

int maple_tree_remove(maple_tree_t *tree, void *key) {
    if (!tree) return 0;
    int removed = 0;
    tree->root = remove_node(tree, tree->root, key, &removed);
    return removed;
}

size_t maple_tree_size(maple_tree_t *tree) {
    return tree ? tree->size : 0;
}

/* --- Iterator --- */

typedef struct maple_tree_iterator_t {
    latte_iterator_t base;
    maple_tree_t *tree;
    // Simple stack-based in-order traversal
    maple_node_t **stack;
    int stack_capacity;
    int stack_top;
    
    latte_pair_t current_pair;
} maple_tree_iterator_t;

static void push_left(maple_tree_iterator_t *it, maple_node_t *node) {
    while (node) {
        if (it->stack_top + 1 >= it->stack_capacity) {
            it->stack_capacity *= 2;
            it->stack = zrealloc(it->stack, sizeof(maple_node_t*) * it->stack_capacity);
        }
        it->stack[++it->stack_top] = node;
        node = node->left;
    }
}

static bool iterator_has_next(latte_iterator_t *base) {
    maple_tree_iterator_t *it = (maple_tree_iterator_t*)base;
    return it->stack_top >= 0;
}

static void* iterator_next(latte_iterator_t *base) {
    maple_tree_iterator_t *it = (maple_tree_iterator_t*)base;
    if (it->stack_top < 0) return NULL;
    
    maple_node_t *node = it->stack[it->stack_top--];
    push_left(it, node->right);
    
    it->current_pair.key = node->key;
    it->current_pair.value = node->value;
    return &it->current_pair;
}


static void iterator_delete_wrapper(latte_iterator_t *base) {
    if (base) {
         maple_tree_iterator_t *it = (maple_tree_iterator_t*)base;
         if (it->stack) zfree(it->stack);
         zfree(it);
    }
}

static latte_iterator_func maple_iterator_functions = {
    .has_next = iterator_has_next,
    .next = iterator_next,
    .release = iterator_delete_wrapper
};

latte_iterator_t* maple_tree_get_iterator(maple_tree_t *tree) {
    if (!tree) return NULL;
    
    maple_tree_iterator_t *it = zmalloc(sizeof(maple_tree_iterator_t));
    if (!it) return NULL;
    
    it->base.func = &maple_iterator_functions;
    it->base.data = NULL; // We don't use generic data, we cast the iterator struct itself
    
    it->tree = tree;
    it->stack_capacity = 32;
    it->stack = zmalloc(sizeof(maple_node_t*) * it->stack_capacity);
    it->stack_top = -1;
    
    push_left(it, tree->root);
    
    return (latte_iterator_t*)it;
}
