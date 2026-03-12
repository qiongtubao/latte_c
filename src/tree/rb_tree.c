
#include "rb_tree.h"
#include "zmalloc/zmalloc.h"
#include <stdio.h>

/**
 * @brief 创建一个新的红黑树
 *
 * 分配 rb_tree_t 结构体，创建哨兵根节点（颜色为 RED，无数据）。
 * @return rb_tree_t* 新建树指针，分配失败返回 NULL
 */
rb_tree_t* rb_tree_new() {
    rb_tree_t* tree = zmalloc(sizeof(rb_tree_t));
    if (tree == NULL) {
        return NULL;
    }
    tree->root = rb_tree_node_new(NULL); /* 创建哨兵根节点 */
    return tree;
}

/**
 * @brief 创建一个新的红黑树节点
 *
 * 分配节点内存，初始化数据、颜色（RED）及左右子节点和父节点指针（均为 NULL）。
 * @param data 节点数据（整型，以 void* 传入）
 * @return rb_tree_node_t* 新建节点指针，分配失败返回 NULL
 */
rb_tree_node_t* rb_tree_node_new(void* data) {
    rb_tree_node_t* node = zmalloc(sizeof(rb_tree_node_t));
    if (node == NULL) {
        return NULL;
    }
    node->data = (int)(long)data; /* 将 void* 转换为整型数据 */
    node->color = RED;            /* 新节点默认为红色 */
    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;
    return node;
}

/**
 * @brief 向红黑树添加整型数据（尚未实现）
 * @param tree 目标树指针
 * @param data 待插入整型数据
 */
void rb_tree_add(rb_tree_t* tree, int data) {
    /* TODO: 实现红黑树插入及旋转/变色逻辑 */
}

/**
 * @brief 中序遍历打印节点信息（内部辅助函数）
 *
 * 按中序（左-根-右）递归打印节点数据和颜色。
 * @param node 当前节点指针（为 NULL 时返回）
 */
void rb_tree_node_debug(rb_tree_node_t* node) {
    if (node != NULL) {
        rb_tree_node_debug(node->left);
        printf("%d (%s)", node->data, node->color == RED ? "RED" : "BLACK");
        rb_tree_node_debug(node->right);
    }
}

/**
 * @brief 打印红黑树的中序遍历调试信息
 * @param tree 目标树指针（为 NULL 时直接返回）
 */
void rb_tree_debug(rb_tree_t* tree) {
    if (tree != NULL && tree->root != NULL) {
        rb_tree_node_debug(tree->root);
    }
}
