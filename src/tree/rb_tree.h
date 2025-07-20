#ifndef __LATTE_RB_TREE_H
#define __LATTE_RB_TREE_H

//
typedef enum rb_color_enum {
    RED,
    BLOCK 
} rb_color_enum;

typedef struct rb_tree_node_t {
    int data;
    rb_color_enum color;
    struct rb_tree_node_t* left, *right, *parent;
} rb_tree_node_t;

typedef struct rb_tree_t {
    rb_tree_node_t* root;
} rb_tree_t;
rb_tree_node_t* rb_tree_node_new();
rb_tree_t* rb_tree_new();

void rb_tree_debug(rb_tree_t* tree);


#endif