#ifndef __LATTE_RB_TREE_H
#define __LATTE_RB_TREE_H

/**
 * @brief 红黑树节点颜色枚举
 */
typedef enum rb_color_enum {
    RED,    /**< 红色节点 */
    BLOCK   /**< 黑色节点（注：拼写为 BLOCK，实为 BLACK） */
} rb_color_enum;

/**
 * @brief 红黑树节点结构体
 *
 * 存储整型数据、颜色及左右子节点和父节点指针，
 * 通过颜色约束维持红黑树的平衡性质。
 */
typedef struct rb_tree_node_t {
    int data;                       /**< 节点存储的整型数据 */
    rb_color_enum color;            /**< 节点颜色（RED/BLOCK） */
    struct rb_tree_node_t* left;    /**< 左子节点 */
    struct rb_tree_node_t* right;   /**< 右子节点 */
    struct rb_tree_node_t* parent;  /**< 父节点 */
} rb_tree_node_t;

/**
 * @brief 红黑树结构体
 */
typedef struct rb_tree_t {
    rb_tree_node_t* root;   /**< 根节点指针（始终为黑色），空树为 NULL */
} rb_tree_t;

/**
 * @brief 创建一个新的红黑树节点
 * @return rb_tree_node_t* 新建节点指针
 */
rb_tree_node_t* rb_tree_node_new();

/**
 * @brief 创建一个新的红黑树
 * @return rb_tree_t* 新建树指针
 */
rb_tree_t* rb_tree_new();

/**
 * @brief 打印红黑树的调试信息（结构可视化）
 * @param tree 目标树指针
 */
void rb_tree_debug(rb_tree_t* tree);

#endif
