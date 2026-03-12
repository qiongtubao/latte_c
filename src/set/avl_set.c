/**
 * @file avl_set.c
 * @brief 基于AVL树实现的Set集合数据结构文件
 * @details 实现了基于AVL平衡二叉树的集合操作，提供高效的增删查功能
 * @author latte_c
 * @date 2026-03-12
 */

#include "avl_set.h"
#include "sds/sds.h"


/**
 * @brief 向底层的AVL树中添加元素，作为Set的add实现
 * @param set 底层的AVL树
 * @param key 要添加的元素
 * @return int 状态码：0表示已存在，1表示新添加
 */
int avl_set_add(avl_set_t* set, void* key) {
    return avl_tree_put(set, key, NULL);
}

/**
 * @brief 检查底层的AVL树是否包含指定的元素
 * @param set 底层的AVL树
 * @param element 要检查的元素
 * @return int 状态码：1表示存在，0表示不存在
 */
int avl_set_contains(avl_set_t* set, void* element) {
    avl_node_t* node = avl_tree_get(set, element);
    return node == NULL? 0 : 1;
}

/**
 * @brief 实现 set_func_t 的 add 接口，向基于AVL树的Set中添加元素
 */
int avl_set_type_add(set_t *set, void* key) {
    return avl_set_add((avl_tree_t *)set->data, key);
}

/**
 * @brief 实现 set_func_t 的 contains 接口，检查基于AVL树的Set是否包含元素
 */
int avl_set_type_contains(set_t *set, void* key) {
    return avl_set_contains((avl_tree_t *)set->data, key);
}

/**
 * @brief 实现 set_func_t 的 remove 接口，从基于AVL树的Set中移除元素
 */
int avl_set_type_remove(set_t* set, void* key) {
    return avl_tree_remove((avl_tree_t *)set->data, key);
}

/**
 * @brief 实现 set_func_t 的 size 接口，获取基于AVL树的Set的元素数量
 */
size_t avl_set_type_size(set_t* set) {
    return avl_tree_size((avl_tree_t *)set->data);
}

/**
 * @brief 实现 set_func_t 的 release 接口，释放基于AVL树的Set对象
 */
void avl_set_type_delete(set_t* set) {
    avl_tree_delete((avl_tree_t *)set->data);
    zfree(set);
}

/**
 * @brief 实现 set_func_t 的 clear 接口，清空基于AVL树的Set中的所有元素
 */
void avl_set_type_clear(set_t* set) {
    avl_tree_clear((avl_tree_t *)set->data);
}

/**
 * @brief 实现 set_func_t 的 getIterator 接口，获取基于AVL树的Set的迭代器
 */
latte_iterator_t* avl_set_type_get_iterator(set_t* set) {
    return avl_tree_get_iterator((avl_tree_t *)set->data);
}

/**
 * @brief AVLSet的虚拟函数表(vtable)，实现了Set的接口
 */
struct set_func_t avl_set_func = {
    .add = avl_set_type_add,
    .contains = avl_set_type_contains,
    .remove = avl_set_type_remove,
    .size = avl_set_type_size,
    .release = avl_set_type_delete,
    .getIterator = avl_set_type_get_iterator,
    .clear = avl_set_type_clear
};

/**
 * @brief 创建一个新的基于AVL树实现的Set
 * @param type AVL树的具体类型操作函数表
 * @return set_t* 返回抽象的Set对象
 */
set_t* avl_set_new(avl_set_func_t* type) {
    set_t* s = zmalloc(sizeof(set_t));
    s->data = avl_tree_new(type);
    s->type = &avl_set_func;
    return s;
}

#define UNUSED(x) (void)x

/**
 * @brief 键为sds字符串时的AVL节点创建函数
 */
avl_node_t* set_sds_node_new(void* key, void* value) {
    UNUSED(value);
    avl_node_t* node = zmalloc(sizeof(avl_node_t));
    node->left = NULL;
    node->right = NULL;
    node->key = sds_dup(key);
    node->height = 0;
    return node;
}


/**
 * @brief 键为sds字符串时的空操作函数，Set不需要设置value
 */
void UNSETVAL(avl_node_t* node, void* val) {
    UNUSED(node);
    UNUSED(val);
}

/**
 * @brief 键为sds字符串时的AVL节点释放函数
 */
void set_sds_node_delete(avl_node_t* node) {
    sds_delete(node->key);
    zfree(node);
}

/**
 * @brief 键为sds字符串时的比较函数
 */
int set_sds_operator(void* a, void* b) {
    return sds_cmp((sds)a, (sds)b);
}

/**
 * @brief 键为sds字符串的AVLSet操作函数表实例
 */
avl_set_func_t avl_set_sds_func = {
    .create_node = set_sds_node_new,
    .node_set_val = UNSETVAL,
    .operator = set_sds_operator,
    .release_node = set_sds_node_delete
};

