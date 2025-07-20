
#include "rb_tree.h"
#include "zmalloc/zmalloc.h"
rb_tree_t* rb_tree_new() {
    rb_tree_t* tree = zmalloc(sizeof(rb_tree_t));
    if (tree == NULL) {
        return NULL;
    }
    tree->root = rb_tree_node_new(NULL);
    return tree;
}

rb_tree_node_t* rb_tree_node_new(void* data) {
    rb_tree_node_t* node = zmalloc(sizeof(rb_tree_node_t));
    if (node == NULL) {
        return NULL;
    }
    node->data = data;
    node->color = RED;
    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;
    return node;
}

void rb_tree_add(rb_tree_t* tree, int data) {

}

void rb_tree_node_debug(rb_tree_node_t* node) {
    if (node != NULL) {
        rb_tree_node_debug(node->left);
        printf("%d (%s)", node->data, node->color);
        rb_tree_node_debug(node->right);
    }
}

void rb_tree_debug(rb_tree_t* tree) {
    if (tree != NULL && tree->root != NULL) {
        rb_tree_node_debug(tree->root);
    }
}