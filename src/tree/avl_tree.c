#include "avl_tree.h"
#include "zmalloc/zmalloc.h"

/**
 * @brief 递归后序释放以 *node 为根的子树（内部辅助函数）
 * 先递归释放左右子树，再调用 type->release_node 释放当前节点，
 * 并将指针置 NULL。
 * @param node 指向节点指针的指针（释放后置 NULL）
 * @param type 虚函数表指针（提供 release_node）
 */
void avl_node_recursively(avl_node_t** node, avl_tree_type_t* type) {
    if (*node != NULL) {
        avl_node_recursively(&(*node)->left, type);   /* 递归释放左子树 */
        avl_node_recursively(&(*node)->right, type);  /* 递归释放右子树 */
        type->release_node(*node);                    /* 释放当前节点 */
        *node = NULL;
    }
}

/**
 * @brief 释放 AVL 树及其所有节点
 * 后序递归释放所有节点后释放树结构体本身。
 * @param tree 目标树指针
 */
void avl_tree_delete(avl_tree_t* tree) {
    avl_node_recursively(&tree->root, tree->type);
    zfree(tree);
}

/**
 * @brief 清空树中所有节点（保留树结构体）
 * @param tree 目标树指针
 */
void avl_tree_clear(avl_tree_t* tree) {
    avl_node_recursively(&tree->root, tree->type);
}

/**
 * @brief 获取节点高度（NULL 节点高度为 0）
 * @param node 目标节点指针（可为 NULL）
 * @return int 节点高度
 */
int avl_node_get_height(avl_node_t* node) {
    if (node == NULL) {
        return 0;
    }
    return node->height;
}

/** @brief 取两值较大者（用于计算节点高度） */
#define max(a,b) a < b? b: a

/**
 * @brief 计算节点的平衡因子（左高 - 右高）
 * @param node 目标节点指针（可为 NULL）
 * @return int 平衡因子；NULL 节点返回 0
 */
int get_balance(avl_node_t* node) {
    if (node == NULL) {
        return 0;
    }
    return avl_node_get_height(node->left) - avl_node_get_height(node->right);
}

/**
 * @brief 右旋转（LL 失衡修复）
 *
 * 以 y 为失衡节点，将其左子 x 提升为新根：
 *   x->right = y，y->left = x->right（原 T2）
 * 更新 y 和 x 的高度。
 * @param y 失衡节点指针
 * @return avl_node_t* 旋转后的新根节点（x）
 */
avl_node_t* rotate_right(avl_node_t* y) {
    avl_node_t* x = y->left;
    avl_node_t* T2 = x->right;

    /* 执行旋转 */
    x->right = y;
    y->left = T2;

    /* 更新高度（先更新 y，再更新 x） */
    y->height = max(avl_node_get_height(y->left), avl_node_get_height(y->right)) + 1;
    x->height = max(avl_node_get_height(x->left), avl_node_get_height(x->right)) + 1;

    return x;
}

/**
 * @brief 左旋转（RR 失衡修复）
 *
 * 以 x 为失衡节点，将其右子 y 提升为新根：
 *   y->left = x，x->right = y->left（原 T2）
 * 更新 x 和 y 的高度。
 * @param x 失衡节点指针
 * @return avl_node_t* 旋转后的新根节点（y）
 */
avl_node_t* rotate_left(avl_node_t* x) {
    avl_node_t* y = x->right;
    avl_node_t* T2 = y->left;

    /* 执行旋转 */
    y->left = x;
    x->right = T2;

    /* 更新高度（先更新 x，再更新 y） */
    x->height = max(avl_node_get_height(x->left), avl_node_get_height(x->right)) + 1;
    y->height = max(avl_node_get_height(y->left), avl_node_get_height(y->right)) + 1;

    return y;
}

/**
 * @brief 在以 node 为根的子树中插入键值对（内部递归函数）
 *
 * 插入后自底向上更新高度并通过旋转维持 AVL 平衡：
 *   - LL 失衡：右旋
 *   - RR 失衡：左旋
 *   - LR 失衡：先左旋左子，再右旋
 *   - RL 失衡：先右旋右子，再左旋
 * 键已存在时调用 type->node_set_val 更新值，*puted 置 0。
 *
 * @param node  当前子树根节点（可为 NULL）
 * @param type  虚函数表指针
 * @param key   待插入键指针
 * @param value 待插入值指针
 * @param puted 输出：1=新增，0=更新，-1=失败
 * @return avl_node_t* 平衡后的子树根节点
 */
avl_node_t* avl_node_insert(avl_node_t* node, avl_tree_type_t* type, void* key, void* value, int* puted) {
    if (node == NULL) {
        /* 到达空位，创建新节点 */
        avl_node_t* node = type->create_node(key, value);
        node->left = node->right = NULL;
        node->height = 1;
        *puted = 1;
        return node;
    }
    int cmp = type->operator(key, node->key);
    if (cmp < 0) {
        node->left = avl_node_insert(node->left, type, key, value, puted);
    } else if (cmp > 0) {
        node->right = avl_node_insert(node->right, type, key, value, puted);
    } else {
        /* 键已存在，更新值 */
        type->node_set_val(node, value);
        *puted = 0;
    }

    /* 更新当前节点高度 */
    node->height = 1 + max(avl_node_get_height(node->left), avl_node_get_height(node->right));

    int balance = get_balance(node);

    /* LL 失衡：右旋 */
    if (balance > 1 && type->operator(key, node->left->key) > 0) {
        return rotate_right(node);
    }
    /* RR 失衡：左旋 */
    if (balance < -1 && type->operator(key, node->right->key) < 0) {
        return rotate_left(node);
    }
    /* LR 失衡：先左旋左子，再右旋 */
    if (balance > 1 && type->operator(key, node->left->key) < 0) {
        node->left = rotate_left(node->left);
        return rotate_right(node);
    }
    /* RL 失衡：先右旋右子，再左旋 */
    if (balance < -1 && type->operator(key, node->right->key) > 0) {
        node->right = rotate_right(node->right);
        return rotate_left(node);
    }
    return node;
}

/**
 * @brief 插入或更新键值对
 * @param tree  目标树指针
 * @param key   键指针
 * @param value 值指针
 * @return int 1=新增，0=更新，-1=失败
 */
int avl_tree_put(avl_tree_t* tree, void* key, void* value) {
    int puted = -1;
    tree->root = avl_node_insert(tree->root, tree->type, key, value, &puted);
    return puted;
}

/**
 * @brief 在以 node 为根的子树中按键查找节点（内部递归函数）
 * @param node 当前子树根节点
 * @param type 虚函数表指针
 * @param key  待查找键指针
 * @return void* 找到返回节点指针，未找到返回 NULL
 */
void* avl_node_search(avl_node_t* node, avl_tree_type_t* type, void* key) {
    if (node == NULL) {
        return NULL;
    }
    int result = type->operator(node->key, key);
    if (result == 0) return node;           /* 找到 */
    if (result < 0) {
        return avl_node_search(node->left, type, key);   /* 键在左子树 */
    }
    return avl_node_search(node->right, type, key);      /* 键在右子树 */
}

/**
 * @brief 按键查找并返回节点（含键值的完整节点指针）
 * @param tree 目标树指针
 * @param key  键指针
 * @return void* 找到返回 avl_node_t* 指针，未找到返回 NULL
 */
void* avl_tree_get(avl_tree_t* tree, void* key) {
    return avl_node_search(tree->root, tree->type, key);
}

/**
 * @brief 查找子树中最小键节点（最左叶子）
 * @param node 子树根节点
 * @return avl_node_t* 最小节点指针
 */
avl_node_t* find_min_value_node(avl_node_t* node) {
    avl_node_t* current = node;
    while (current && current->left != NULL) {
        current = current->left;
    }
    return current;
}

/**
 * @brief 获取树中最小键节点
 * @param tree 目标树指针
 * @return avl_node_t* 最小节点指针，空树返回 NULL
 */
avl_node_t* avl_tree_get_min(avl_tree_t* tree) {
    return find_min_value_node(tree->root);
}

/**
 * @brief 在以 node 为根的子树中删除指定键（内部递归函数）
 *
 * 删除后自底向上更新高度并通过旋转维持 AVL 平衡。
 * 对有两个子节点的情况，用右子树最小节点替换后递归删除。
 *
 * @param node    当前子树根节点
 * @param type    虚函数表指针
 * @param key     待删除键指针
 * @param deleted 输出：1=找到并删除
 * @return avl_node_t* 平衡后的子树根节点
 */
avl_node_t* delete_child_node(avl_node_t* node, avl_tree_type_t* type, void* key, int* deleted) {
    if (node == NULL) return node;

    int result = type->operator(key, node->key);
    if (result > 0) {
        /* 目标键在左子树 */
        node->left = delete_child_node(node->left, type, key, deleted);
    } else if (result < 0) {
        /* 目标键在右子树 */
        node->right = delete_child_node(node->right, type, key, deleted);
    } else {
        /* 找到目标节点，执行删除 */
        if ((node->left == NULL) || (node->right == NULL)) {
            /* 至多一个子节点：直接用子节点替换 */
            avl_node_t* temp = node->left ? node->left : node->right;
            if (temp == NULL) {
                /* 叶子节点：直接删除 */
                temp = node;
                node = NULL;
            } else {
                *node = *temp; /* 复制子节点内容到当前位置 */
            }
            *deleted = 1;
            type->release_node(temp);
        } else {
            /* 有两个子节点：用右子树最小节点替换键，然后递归删除右子树中该最小节点 */
            avl_node_t* temp = find_min_value_node(node->right);
            node->key = temp->key;
            node->right = delete_child_node(node->right, type, temp->key, deleted);
        }
    }

    if (node == NULL) return node;

    /* 更新高度 */
    node->height = 1 + max(avl_node_get_height(node->left), avl_node_get_height(node->right));

    int balance = get_balance(node);

    /* LL 失衡：右旋 */
    if (balance > 1 && get_balance(node->left) >= 0) {
        return rotate_right(node);
    }
    /* LR 失衡：先左旋左子，再右旋 */
    if (balance > 1 && get_balance(node->left) < 0) {
        node->left = rotate_left(node->left);
        return rotate_right(node);
    }
    /* RR 失衡：左旋 */
    if (balance < -1 && get_balance(node->right) <= 0) {
        return rotate_left(node);
    }
    /* RL 失衡：先右旋右子，再左旋 */
    if (balance < -1 && get_balance(node->right) > 0) {
        node->right = rotate_right(node->right);
        return rotate_left(node);
    }
    return node;
}

/**
 * @brief 按键删除节点
 * @param tree 目标树指针
 * @param key  要删除的键指针
 * @return int 成功删除返回 1，未找到返回 0
 */
int avl_tree_remove(avl_tree_t* tree, void* key) {
    int delete = 0;
    tree->root = delete_child_node(tree->root, tree->type, key, &delete);
    return delete;
}

/**
 * @brief 递归统计以 node 为根的子树节点数
 * @param node 子树根节点
 * @return int 节点数量
 */
int node_length(avl_node_t* node) {
    if (node == NULL) {
        return 0;
    }
    return 1 + node_length(node->left) + node_length(node->right);
}

/**
 * @brief 获取树中节点总数
 * @param tree 目标树指针
 * @return int 节点数量
 */
int avl_tree_size(avl_tree_t* tree) {
    return node_length(tree->root);
}

/**
 * @brief 创建 AVL 树中序迭代器内部状态（内部辅助函数）
 *
 * 分配显式栈并将所有左子节点压栈，使迭代器初始指向最小节点。
 * @param tree 目标树指针
 * @return avl_tree_iterator_t* 新建迭代器内部状态
 */
avl_tree_iterator_t* avl_tree_iterator_new(avl_tree_t* tree) {
    avl_tree_iterator_t* it = zmalloc(sizeof(avl_tree_iterator_t));
    it->current = tree->root;
    it->stackSize = 16; /* 初始栈容量 */
    it->stack = zmalloc(it->stackSize * sizeof(avl_node_t*));
    it->top = 0;

    /* 将根到最左叶子路径上的所有节点压栈 */
    while (it->current != NULL) {
        it->stack[it->top++] = it->current;
        /* 栈满时动态扩容 */
        if (it->top == it->stackSize) {
            it->stackSize *= 2;
            it->stack = zrealloc(it->stack, it->stackSize * sizeof(avl_node_t*));
        }
        it->current = it->current->left;
    }
    return it;
}

/**
 * @brief 中序迭代器的 next 函数：返回下一个键值对并推进迭代状态
 *
 * 弹出栈顶节点作为当前访问节点，然后将其右子树的最左路径压栈。
 * @param it_ 通用迭代器指针
 * @return void* 当前键值对（latte_pair_t*），无更多节点返回 NULL
 */
void* avl_tree_iterator_next(latte_iterator_t* it_) {
    avl_tree_iterator_t* data = it_->data;
    if (data->top <= 0) {
        return NULL; /* 栈为空，迭代结束 */
    }
    avl_node_t* node = data->stack[--data->top]; /* 弹出栈顶节点 */
    data->current = node->right;
    /* 将右子树的最左路径压栈 */
    while(data->current != NULL) {
        data->stack[data->top++] = data->current;
        if (data->top == data->stackSize) {
            data->stackSize *= 2;
            data->stack = zrealloc(data->stack, data->stackSize * sizeof(avl_node_t*));
        }
        data->current = data->current->left;
    }
    avl_tree_latte_iterator_t* iterator = (avl_tree_latte_iterator_t*)it_;
    iterator->pair.key = node->key;
    iterator->pair.value = iterator->tree->type->get_value == NULL ? NULL
                         : iterator->tree->type->get_value(node); /* 通过 get_value 获取值 */
    return &iterator->pair;
}

/**
 * @brief 中序迭代器的 release 函数：释放迭代器内存
 * @param iterator 通用迭代器指针
 */
void avl_tree_iterator_delete(latte_iterator_t* iterator) {
    avl_tree_iterator_t* data = iterator->data;
    zfree(data->stack);
    zfree(data);
    zfree(iterator);
}

/**
 * @brief 中序迭代器的 has_next 函数：判断是否还有未访问节点
 * @param iterator 通用迭代器指针
 * @return bool 栈非空返回 true
 */
bool avl_tree_iterator_has_next(latte_iterator_t* iterator) {
    avl_tree_iterator_t* data = iterator->data;
    return data->top > 0;
}

/** @brief AVL 树中序迭代器虚函数表 */
latte_iterator_func avl_tree_iterator_tType = {
     .next = avl_tree_iterator_next,
     .release = avl_tree_iterator_delete,
     .has_next = avl_tree_iterator_has_next
};

/**
 * @brief 获取 AVL 树的中序迭代器（升序遍历）
 *
 * 分配 avl_tree_latte_iterator_t，嵌入通用迭代器和键值对字段，
 * 通过类型转换兼容 latte_iterator_t 接口。
 * @param tree 目标树指针
 * @return latte_iterator_t* 新建迭代器（调用方负责释放）
 */
latte_iterator_t* avl_tree_get_iterator(avl_tree_t* tree) {
    avl_tree_latte_iterator_t* iterator = zmalloc(sizeof(avl_tree_latte_iterator_t));
    iterator->iterator.func = &avl_tree_iterator_tType;
    iterator->iterator.data = avl_tree_iterator_new(tree); /* 初始化内部栈状态 */
    iterator->tree  = tree;
    iterator->pair.key = NULL;
    iterator->pair.value = NULL;
    return (latte_iterator_t*)iterator;
}

/**
 * @brief 创建一个新的 AVL 树
 * @param type 虚函数表指针（必须非 NULL）
 * @return avl_tree_t* 新建树指针（根节点为 NULL）
 */
avl_tree_t* avl_tree_new(avl_tree_type_t* type) {
    avl_tree_t* tree = zmalloc(sizeof(avl_tree_t));
    tree->type = type;
    tree->root = NULL;
    return tree;
}
