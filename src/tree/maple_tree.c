#include "maple_tree.h"
#include "zmalloc/zmalloc.h"
#include <stdlib.h>
#include <stdio.h>

/**
 * @brief 创建一个新的 Maple 树节点（内部辅助函数）
 *
 * 分配节点内存，初始化键值和左右子节点为 NULL，
 * 使用 rand() 生成随机优先级以维持 Treap 堆性质。
 * @param key   键指针
 * @param value 值指针
 * @return maple_node_t* 新建节点指针，分配失败返回 NULL
 */
static maple_node_t* create_node(void *key, void *value) {
    maple_node_t *node = zmalloc(sizeof(maple_node_t));
    if (!node) return NULL;
    node->key = key;
    node->value = value;
    node->left = NULL;
    node->right = NULL;
    node->priority = rand(); /* 随机优先级，维持 Treap 堆性质 */
    return node;
}

/**
 * @brief 递归后序释放以 node 为根的子树（内部辅助函数）
 *
 * 先递归释放左右子树，然后调用 type->release_key/release_value
 * 释放键值内存，最后释放节点结构体。
 * @param tree 目标树指针（提供释放函数）
 * @param node 待释放的子树根节点（为 NULL 时直接返回）
 */
static void release_node_recursive(maple_tree_t *tree, maple_node_t *node) {
    if (!node) return;
    release_node_recursive(tree, node->left);  /* 递归释放左子树 */
    release_node_recursive(tree, node->right); /* 递归释放右子树 */

    if (tree->type->release_key) {
        tree->type->release_key(node->key);   /* 释放键内存 */
    }
    if (tree->type->release_value) {
        tree->type->release_value(node->value); /* 释放值内存 */
    }
    zfree(node);
}

/**
 * @brief 右旋转（内部辅助函数，用于维持 Treap 堆性质）
 *
 * 将 y 的左子 x 提升为新根：x->right = y，y->left = x->right(T2)
 * @param y 旋转前的根节点
 * @return maple_node_t* 旋转后的新根节点（x）
 */
static maple_node_t* rotate_right(maple_node_t *y) {
    maple_node_t *x = y->left;
    maple_node_t *T2 = x->right;

    x->right = y;
    y->left = T2;

    return x;
}

/**
 * @brief 左旋转（内部辅助函数，用于维持 Treap 堆性质）
 *
 * 将 x 的右子 y 提升为新根：y->left = x，x->right = y->left(T2)
 * @param x 旋转前的根节点
 * @return maple_node_t* 旋转后的新根节点（y）
 */
static maple_node_t* rotate_left(maple_node_t *x) {
    maple_node_t *y = x->right;
    maple_node_t *T2 = y->left;

    y->left = x;
    x->right = T2;

    return y;
}

/**
 * @brief 向以 node 为根的子树中插入键值对（内部递归函数）
 *
 * 按 BST 性质递归找到插入位置，创建新节点后通过旋转维持 Treap 堆性质
 * （父节点优先级 >= 子节点优先级）。键已存在时更新值。
 * @param tree  目标树指针
 * @param node  当前子树根节点（可为 NULL）
 * @param key   待插入键
 * @param value 待插入值
 * @param added 输出：1=新增，0=更新
 * @return maple_node_t* 调整后的子树根节点
 */
static maple_node_t* insert_node(maple_tree_t *tree, maple_node_t *node, void *key, void *value, int *added) {
    if (!node) {
        /* 到达空位，创建新节点 */
        *added = 1;
        tree->size++;
        return create_node(key, value);
    }

    int cmp = tree->type->compare(key, node->key);

    if (cmp < 0) {
        node->left = insert_node(tree, node->left, key, value, added);
        /* 若左子优先级更高，右旋恢复堆性质 */
        if (node->left->priority > node->priority) {
            node = rotate_right(node);
        }
    } else if (cmp > 0) {
        node->right = insert_node(tree, node->right, key, value, added);
        /* 若右子优先级更高，左旋恢复堆性质 */
        if (node->right->priority > node->priority) {
            node = rotate_left(node);
        }
    } else {
        /* 键已存在：更新值 */
        if (tree->type->release_value) {
            tree->type->release_value(node->value);
        }
        node->value = value;
        *added = 0;
    }
    return node;
}

/**
 * @brief 从以 node 为根的子树中删除指定键（内部递归函数）
 *
 * 找到目标节点后：
 *   - 无左子：直接用右子替换
 *   - 无右子：直接用左子替换
 *   - 有两个子节点：根据优先级向优先级较高的子节点方向旋转后递归删除
 * @param tree    目标树指针
 * @param node    当前子树根节点
 * @param key     待删除键
 * @param removed 输出：1=找到并删除，0=未找到
 * @return maple_node_t* 调整后的子树根节点
 */
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
        /* 找到目标节点 */
        *removed = 1;
        if (!node->left) {
            /* 无左子：直接用右子替换 */
            maple_node_t *temp = node->right;
            if (tree->type->release_key) tree->type->release_key(node->key);
            if (tree->type->release_value) tree->type->release_value(node->value);
            zfree(node);
            tree->size--;
            return temp;
        } else if (!node->right) {
            /* 无右子：直接用左子替换 */
            maple_node_t *temp = node->left;
            if (tree->type->release_key) tree->type->release_key(node->key);
            if (tree->type->release_value) tree->type->release_value(node->value);
            zfree(node);
            tree->size--;
            return temp;
        }

        /* 有两个子节点：向优先级较高方向旋转后递归删除 */
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

/**
 * @brief 创建一个新的 Maple 树
 *
 * 分配 maple_tree_t，初始化根节点为 NULL，size 为 0。
 * type 和 type->compare 均必须非 NULL。
 * @param type 操作函数表指针
 * @return maple_tree_t* 新建树指针，参数无效或分配失败返回 NULL
 */
maple_tree_t* maple_tree_new(maple_tree_type_t *type) {
    if (!type || !type->compare) return NULL;

    maple_tree_t *tree = zmalloc(sizeof(maple_tree_t));
    if (!tree) return NULL;

    tree->root = NULL;
    tree->type = type;
    tree->size = 0;
    return tree;
}

/**
 * @brief 释放 Maple 树及其所有节点
 *
 * 先调用 maple_tree_clear 递归释放所有节点，再释放树结构体。
 * @param tree 目标树指针（为 NULL 时直接返回）
 */
void maple_tree_delete(maple_tree_t *tree) {
    if (!tree) return;
    maple_tree_clear(tree);
    zfree(tree);
}

/**
 * @brief 清空树中所有节点（树结构体保留）
 *
 * 递归释放所有节点，将 root 置 NULL，size 置 0。
 * @param tree 目标树指针（为 NULL 时直接返回）
 */
void maple_tree_clear(maple_tree_t *tree) {
    if (!tree) return;
    release_node_recursive(tree, tree->root);
    tree->root = NULL;
    tree->size = 0;
}

/**
 * @brief 插入或更新键值对
 *
 * 新键时新增节点（size++），已有键时释放旧值并更新。
 * @param tree  目标树指针
 * @param key   键指针
 * @param value 值指针
 * @return int 新增返回 1，更新返回 0，tree 为 NULL 返回 0
 */
int maple_tree_put(maple_tree_t *tree, void *key, void *value) {
    if (!tree) return 0;
    int added = 0;
    tree->root = insert_node(tree, tree->root, key, value, &added);
    return added;
}

/**
 * @brief 按键查找值（迭代版本，O(log N) 期望复杂度）
 * @param tree 目标树指针
 * @param key  键指针
 * @return void* 找到返回值指针，未找到或 tree 为 NULL 返回 NULL
 */
void* maple_tree_get(maple_tree_t *tree, void *key) {
    if (!tree || !tree->root) return NULL;

    maple_node_t *curr = tree->root;
    while (curr) {
        int cmp = tree->type->compare(key, curr->key);
        if (cmp == 0) return curr->value; /* 找到 */
        if (cmp < 0) curr = curr->left;   /* 目标在左子树 */
        else curr = curr->right;           /* 目标在右子树 */
    }
    return NULL;
}

/**
 * @brief 按键删除节点
 * @param tree 目标树指针
 * @param key  待删除键
 * @return int 成功删除返回 1，未找到返回 0，tree 为 NULL 返回 0
 */
int maple_tree_remove(maple_tree_t *tree, void *key) {
    if (!tree) return 0;
    int removed = 0;
    tree->root = remove_node(tree, tree->root, key, &removed);
    return removed;
}

/**
 * @brief 获取树中节点总数
 * @param tree 目标树指针
 * @return size_t 节点数量，tree 为 NULL 返回 0
 */
size_t maple_tree_size(maple_tree_t *tree) {
    return tree ? tree->size : 0;
}

/**
 * @brief Maple 树中序迭代器内部状态（基于显式栈的中序遍历）
 */
typedef struct maple_tree_iterator_t {
    latte_iterator_t base;          /**< 通用迭代器基类（必须为第一个字段，支持类型转换） */
    maple_tree_t *tree;             /**< 所属 Maple 树指针 */
    maple_node_t **stack;           /**< 显式栈（存储待访问节点指针） */
    int stack_capacity;             /**< 栈的总容量 */
    int stack_top;                  /**< 栈顶索引（-1 表示空栈） */
    latte_pair_t current_pair;      /**< 当前迭代到的键值对 */
} maple_tree_iterator_t;

/**
 * @brief 将节点及其所有左子节点依次压栈（内部辅助函数）
 *
 * 用于初始化迭代器和 next 时推进到下一个中序节点。
 * 若栈满则自动扩容至 2 倍。
 * @param it   迭代器指针
 * @param node 起始节点
 */
static void push_left(maple_tree_iterator_t *it, maple_node_t *node) {
    while (node) {
        /* 栈满时动态扩容 */
        if (it->stack_top + 1 >= it->stack_capacity) {
            it->stack_capacity *= 2;
            it->stack = zrealloc(it->stack, sizeof(maple_node_t*) * it->stack_capacity);
        }
        it->stack[++it->stack_top] = node;
        node = node->left;
    }
}

/**
 * @brief 迭代器 has_next 函数：判断是否还有未访问节点
 * @param base 通用迭代器指针
 * @return bool 栈非空返回 true
 */
static bool iterator_has_next(latte_iterator_t *base) {
    maple_tree_iterator_t *it = (maple_tree_iterator_t*)base;
    return it->stack_top >= 0;
}

/**
 * @brief 迭代器 next 函数：返回当前键值对并推进迭代状态
 *
 * 弹出栈顶节点，将其右子树的最左路径压栈，填充 current_pair。
 * @param base 通用迭代器指针
 * @return void* 当前键值对（latte_pair_t*），栈空返回 NULL
 */
static void* iterator_next(latte_iterator_t *base) {
    maple_tree_iterator_t *it = (maple_tree_iterator_t*)base;
    if (it->stack_top < 0) return NULL;

    maple_node_t *node = it->stack[it->stack_top--]; /* 弹出栈顶 */
    push_left(it, node->right);                       /* 推入右子树最左路径 */

    it->current_pair.key = node->key;
    it->current_pair.value = node->value;
    return &it->current_pair;
}

/**
 * @brief 迭代器 release 函数：释放迭代器内存
 * @param base 通用迭代器指针（为 NULL 时直接返回）
 */
static void iterator_delete_wrapper(latte_iterator_t *base) {
    if (base) {
        maple_tree_iterator_t *it = (maple_tree_iterator_t*)base;
        if (it->stack) zfree(it->stack);
        zfree(it);
    }
}

/** @brief Maple 树迭代器虚函数表 */
static latte_iterator_func maple_iterator_functions = {
    .has_next = iterator_has_next,
    .next = iterator_next,
    .release = iterator_delete_wrapper
};

/**
 * @brief 获取 Maple 树的中序迭代器（升序遍历）
 *
 * 分配迭代器结构体，初始化显式栈容量为 32，
 * 将根节点到最左叶子的路径压栈作为初始状态。
 * @param tree 目标树指针
 * @return latte_iterator_t* 新建迭代器（调用方负责释放），失败返回 NULL
 */
latte_iterator_t* maple_tree_get_iterator(maple_tree_t *tree) {
    if (!tree) return NULL;

    maple_tree_iterator_t *it = zmalloc(sizeof(maple_tree_iterator_t));
    if (!it) return NULL;

    it->base.func = &maple_iterator_functions;
    it->base.data = NULL; /* 迭代器自身即为数据载体，不使用 data 字段 */

    it->tree = tree;
    it->stack_capacity = 32; /* 初始栈容量 */
    it->stack = zmalloc(sizeof(maple_node_t*) * it->stack_capacity);
    it->stack_top = -1;      /* 空栈 */

    /* 将根到最左叶子路径压栈，迭代器初始指向最小节点 */
    push_left(it, tree->root);

    return (latte_iterator_t*)it;
}
