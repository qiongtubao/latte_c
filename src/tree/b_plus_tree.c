#include "b_plus_tree.h"
#include <stdbool.h>
#include <stdlib.h>
#include "zmalloc/zmalloc.h"
#include "utils/utils.h"
#include <string.h>
#include "log/log.h"

/**
 * @brief 创建一个新的 B+ 树
 * @param type  操作函数表指针（cmp/free_node）
 * @param order 树的阶（每个节点最多 order 个子节点）
 * @return b_plus_tree_t* 新建树指针，分配失败返回 NULL
 */
b_plus_tree_t* b_plus_tree_new(b_plus_tree_func_t* type, int order) {
    b_plus_tree_t* root = zmalloc(sizeof(b_plus_tree_t));
    if (root == NULL) return NULL;
    root->root = NULL;
    root->type = type;
    root->order = order;
    return root;
}

/**
 * @brief 创建一个新的 B+ 树节点
 *
 * 分配节点结构体，初始化 keys/data/children 数组（全置 NULL），
 * next 置 NULL，numKeys 置 0。
 * @param order  树的阶（决定数组容量：keys/data 容量=order，children 容量=order+1）
 * @param isLeaf 是否为叶子节点（1=叶子）
 * @return b_plus_tree_node_t* 新建节点指针
 */
b_plus_tree_node_t* b_plus_tree_node_new(int order, int isLeaf) {
    latte_assert_with_info(order > 1, "b_plus_tree_node_new order <= 1");
    b_plus_tree_node_t* node = zmalloc(sizeof(b_plus_tree_node_t));
    node->isLeaf = isLeaf;
    node->numKeys = 0;
    node->keys = zmalloc(sizeof(void*) * (order));
    node->data = zmalloc(sizeof(void*) * (order));
    node->children = zmalloc(sizeof(b_plus_tree_node_t*) * (order + 1));
    /* 初始化数组为 NULL */
    for(int i = 0; i < order; i++) {
        node->keys[i] = NULL;
        node->data[i] = NULL;
        node->children[i] = NULL;
    }
    node->children[order] = NULL;
    node->next = NULL;
    return node;
}

/**
 * @brief 递归释放 B+ 树节点及其所有子节点
 *
 * 对非叶子节点递归释放 children，然后释放 keys/data/children 数组
 * 及节点结构体本身（不调用 free_node 释放数据内容）。
 * @param node 目标节点指针（为 NULL 时直接返回）
 */
void b_plus_tree_node_delete(b_plus_tree_node_t* node) {
    if (node == NULL) return;

    for (int i = 0; i < node->numKeys; i++) {
        b_plus_tree_node_delete(node->children[i]);
    }
    zfree(node->keys);
    zfree(node->data);
    zfree(node->children);
    zfree(node);
}

/**
 * @brief 向未满节点插入键值对（内部辅助函数）
 *
 * 通过比较找到插入位置 i，将 i 之后的键值及子节点向后移动，
 * 然后写入新键值。
 * @param node 目标节点（必须未满）
 * @param key  待插入键
 * @param data 待插入数据
 * @param cmp  键比较函数
 */
void b_plus_tree_node_insert_non_full(b_plus_tree_node_t* node, void* key, void* data, cmp_func cmp) {
    int i = node->numKeys - 1;
    if (!node->isLeaf) {
        /* 内部节点：先找到子节点索引 */
        while( i >= 0 && cmp(key, node->keys[i]) < 0) {
            i--;
        }
        i++;
        node->children[i + 1] = node->children[i];
    }
    /* 找到插入位置，向后移动键值 */
    while (i >= 0 && cmp(key, node->keys[i]) < 0) {
        i--;
    }
    i++;
    for (int j = node->numKeys; j > i; j--) {
        if (!node->isLeaf) {
            node->children[j] = node->children[j - 1]; /* 移动子节点指针 */
        }
        node->keys[j] = node->keys[j - 1];
        node->data[j] = node->data[j - 1];
    }
    if (!node->isLeaf) {
        node->children[i] = node->children[i - 1];
    }
    LATTE_LIB_LOG(LOG_DEBUG, "insert key: %p, value: %p ,index: %d", key, data, i);
    node->keys[i] = key;
    node->data[i] = data;
    node->numKeys++;
}

/**
 * @brief 分裂满子节点并插入新键值（旧版分裂辅助函数）
 *
 * 将 parent->children[index]（满节点）分裂为两个节点，
 * 中间键提升到父节点，然后将新键值插入合适的子节点。
 * @param tree   目标树指针
 * @param parent 父节点
 * @param index  被分裂子节点在父节点中的索引
 * @param key    待插入键
 * @param data   待插入数据
 */
void b_plus_tree_split_child(b_plus_tree_t* tree, b_plus_tree_node_t* parent, int index, void* key, void* data) {
    b_plus_tree_node_t* child = parent->children[index];
    b_plus_tree_node_t* newChild = b_plus_tree_node_new(tree->order, child->isLeaf);
    int t = tree->order / 2;
    /* 将 child 后半部分键值复制到 newChild */
    for (int j = 0; j < t - 1; j++) {
        newChild->keys[j] = child->keys[t + j];
        newChild->data[j] = child->data[t + j];
    }
    if (!child->isLeaf) {
        for(int j = 0; j <= t - 1; j++) {
            newChild->children[j] = child->children[t + j];
        }
    }
    newChild->numKeys = t - 1;
    child->numKeys = t - 1;

    /* 在父节点中腾出位置并插入新子节点 */
    for (int i = parent->numKeys - 1; i >= index + 1; i--) {
        parent->children[i + 1] = parent->children[i];
    }
    parent->children[index + 1] = newChild;

    /* 提升中间键到父节点 */
    parent->keys[parent->numKeys] = child->keys[t - 1];
    parent->data[parent->numKeys] = child->data[t - 1];
    parent->numKeys++;

    /* 根据新键值大小决定插入哪个子节点 */
    if (tree->type->cmp(key, child->keys[t - 1]) > 0) {
        b_plus_tree_node_insert_non_full(newChild, key, data, tree->type->cmp);
    } else {
        b_plus_tree_node_insert_non_full(child, key, data, tree->type->cmp);
    }
}

/**
 * @brief 将满子节点分裂为两个节点，并更新父节点（主分裂函数）
 *
 * 按照 B+ 树规则分裂 child_node：
 *   - 叶子节点：新节点获得后半部分键值，通过 next 链接，父节点获得新节点首键
 *   - 内部节点：新节点获得后半部分键值和子节点指针，父节点获得中间键
 *
 * 父节点在 index 位置插入新子节点和提升键。
 *
 * @param parent_node 父节点
 * @param tree        目标树指针
 * @param child_node  待分裂的满子节点
 * @param index       child_node 在父节点 children 数组中的索引
 */
void b_plus_tree_node_split(b_plus_tree_node_t* parent_node, b_plus_tree_t *tree, b_plus_tree_node_t* child_node, int index) {
    b_plus_tree_node_t *new_node = b_plus_tree_node_new(tree->order, child_node->isLeaf);

    if (child_node->isLeaf) {
        /* 叶子节点分裂：
         *   order=2: 老节点保留 1 个，新节点获得 1 个
         *   order=3: 老节点保留 2 个，新节点获得 1 个
         *   order=4: 老节点保留 2 个，新节点获得 2 个 */
        for(int i = 0; i < tree->order/2; i++) {
            new_node->keys[i] = child_node->keys[i + (tree->order + 1) / 2];
            new_node->data[i] = child_node->data[i + (tree->order + 1) / 2];
        }
        child_node->numKeys = (tree->order + 1) / 2;
        new_node->numKeys = (tree->order) / 2;
        /* 维护叶子节点链表 */
        new_node->next = child_node->next;
        child_node->next = new_node;
    } else {
        /* 内部节点分裂：
         *   order=2: 新节点获得 1 个子节点，0 个键
         *   order=3: 新节点获得 2 个子节点，1 个键
         *   order=4: 新节点获得 2 个子节点，1 个键 */
        for(int i = 0; i < (tree->order + 1) / 2; i++) {
            new_node->children[i] = child_node->children[i + (tree->order) / 2 + 1];
        }
        for(int i = 0; i < (tree->order - 1) / 2; i++) {
            new_node->keys[i] = child_node->keys[i + (tree->order) / 2 + 1];
            new_node->data[i] = child_node->data[i + (tree->order) / 2 + 1];
        }
        child_node->numKeys = (tree->order) / 2;
        new_node->numKeys = (tree->order - 1) / 2;
    }

    /* 在父节点的 index 位置腾出空间，插入提升键和新子节点指针 */
    for (int i = parent_node->numKeys; i > index; i--) {
        printf("split move child node set key %d = %d\n", i, parent_node->keys[i - 1]);
        parent_node->keys[i] = parent_node->keys[i - 1];
        parent_node->data[i] = parent_node->data[i - 1];
        parent_node->children[i + 1] = parent_node->children[i];
    }
    parent_node->children[index + 1] = new_node;
    latte_assert_with_info(child_node->numKeys != 0, "child_node numkeys != 0\n");
    /* 将 child_node 的中间键提升到父节点的 index 位置 */
    parent_node->keys[index] = child_node->keys[tree->order / 2];
    parent_node->data[index] = child_node->data[tree->order / 2];
    parent_node->numKeys++;
}

/**
 * @brief 向节点递归插入键值对（内部函数）
 *
 * 叶子节点：找到有序插入位置，向后移动并写入。
 * 内部节点：找到目标子节点索引，若子节点满则先分裂，再递归插入。
 * @param node 当前节点（必须非满）
 * @param tree 目标树指针
 * @param key  待插入键
 * @param data 待插入数据
 */
void b_plus_tree_node_insert(b_plus_tree_node_t* node, b_plus_tree_t *tree, void* key, void* data) {
    int i = node->numKeys - 1;

    if (node->isLeaf) {
        /* 叶子节点：从后向前找插入位置，同时移动数据 */
        printf("[insert leaf node] move start %d \n", i);
        while (i >= 0 && tree->type->cmp(key, node->keys[i]) < 0) {
            printf("[insert leaf node] set %d = %d \n", i+1, node->keys[i]);
            node->keys[i + 1] = node->keys[i];
            node->data[i + 1] = node->data[i];
            i--;
        }
        printf("[insert leaf node] move end %d , set %d = %d\n", i, i + 1, key);
        node->keys[i + 1] = key;
        node->data[i + 1] = data;
        node->numKeys++;
    } else {
        /* 内部节点：找到目标子节点 children[i] */
        printf("[insert no leaf node] find start %d %d\n", i, node->keys[i]);
        while (i >= 0 && tree->type->cmp(key, node->keys[i]) < 0) {
            i--;
        }
        i++;
        printf("[insert no leaf node] find %p %d %p %d %d %d\n", node, i, node->children[i], node->numKeys, key, node->keys[i-1]);
        /* 子节点满了：先分裂，再确定插入哪个子节点 */
        if (node->children[i]->numKeys == tree->order) {
            printf("[insert no leaf node] split %d \n", i);
            b_plus_tree_node_split(node, tree, node->children[i], i);
            /* 分裂后判断插入左子还是右子 */
            if (tree->type->cmp(key, node->keys[i]) > 0) {
                i++;
            }
        }
        printf("[insert no leaf node] insert %d \n", i);
        b_plus_tree_node_insert(node->children[i], tree, key, data);
    }
}

/**
 * @brief 向 B+ 树插入键值对
 *
 * 空树时创建根叶子节点直接插入；
 * 根节点满时先创建新根并分裂旧根，再插入。
 * @param tree 目标树指针
 * @param key  待插入键
 * @param data 待插入数据
 */
void b_plus_tree_insert(b_plus_tree_t* tree, void* key, void* data) {
    if (tree->root == NULL) {
        /* 空树：创建根（同时是叶子节点）并插入 */
        tree->root = b_plus_tree_node_new(tree->order, true);
        b_plus_tree_node_insert(tree->root, tree, key, data);
    } else {
        if (tree->root->numKeys == tree->order) {
            /* 根节点满：创建新根，分裂旧根，再插入 */
            b_plus_tree_node_t *new_root = b_plus_tree_node_new(tree->order, false);
            new_root->children[0] = tree->root;
            b_plus_tree_node_split(new_root, tree, tree->root, 0);
            tree->root = new_root;
            b_plus_tree_node_insert(tree->root, tree, key, data);
        } else {
            b_plus_tree_node_insert(tree->root, tree, key, data);
        }
    }
}

/**
 * @brief 递归在子树中查找键对应的数据指针（内部函数）
 *
 * 内部节点：找到合适子节点递归；叶子节点：线性比较找到精确匹配。
 * @param tree 目标树指针
 * @param node 当前节点
 * @param key  待查找键
 * @return void** 找到返回数据指针的地址，未找到返回 NULL
 */
void** private_b_plus_tree_find(b_plus_tree_t* tree, b_plus_tree_node_t* node, void* key) {
    int i = node->numKeys - 1;
    /* 从右向左找到第一个 <= key 的位置 */
    while (i >= 0 && tree->type->cmp(key, node->keys[i]) < 0) {
        i--;
    }
    if (node->isLeaf) {
        /* 叶子节点：精确匹配 */
        if (i < node->numKeys && tree->type->cmp(key, node->keys[i]) == 0) {
            return &node->data[i];
        }
        return NULL;
    } else {
        i++; /* 内部节点：进入下一层 */
    }
    return private_b_plus_tree_find(tree, node->children[i], key);
}

/**
 * @brief 按键查找数据记录
 * @param tree 目标树指针
 * @param key  待查找键
 * @return void* 找到返回数据记录指针，未找到返回 NULL
 */
void* b_plus_tree_find(b_plus_tree_t* tree, void* key) {
    void** resultPoint = private_b_plus_tree_find(tree, tree->root, key);
    if (resultPoint == NULL) return NULL;
    return *resultPoint;
}

/**
 * @brief 更新指定键对应的数据记录
 *
 * 若 free_node 不为 NULL，先释放旧数据，再写入新数据。
 * 键不存在时断言失败。
 * @param tree 目标树指针
 * @param key  待更新键
 * @param data 新数据记录指针
 */
void b_plus_tree_update(b_plus_tree_t* tree, void* key, void* data) {
    latte_assert_with_info(tree->root != NULL, "tree not data");
    void** oldData = private_b_plus_tree_find(tree, tree->root, key);
    latte_assert_with_info(oldData != NULL, "bplus tree update find fail!");
    if (tree->type->free_node != NULL) tree->type->free_node(*oldData); /* 释放旧数据 */
    *oldData = data;
}

/**
 * @brief 插入或更新键值对
 *
 * 键存在时更新数据（可能释放旧数据），键不存在时直接插入。
 * @param tree 目标树指针
 * @param key  键指针
 * @param data 数据记录指针
 */
void b_plus_tree_insert_or_update(b_plus_tree_t* tree, void* key, void* data) {
    if (tree->root != NULL) {
        void** oldData = private_b_plus_tree_find(tree, tree->root, key);
        if (oldData != NULL) {
            /* 键已存在：更新 */
            if (tree->type->free_node != NULL) tree->type->free_node(*oldData);
            *oldData = data;
        } else {
            b_plus_tree_insert(tree, key, data);
        }
    } else {
        b_plus_tree_insert(tree, key, data);
    }
}

/**
 * @brief 创建 tree_node_info_t 辅助结构体
 * @return tree_node_info_t* 新建结构体指针，分配失败返回 NULL
 */
tree_node_info_t* tree_node_info_new() {
    tree_node_info_t* info = zmalloc(sizeof(tree_node_info_t));
    if (info == NULL) return NULL;
    info->index = 0;
    info->parent = NULL;
    info->leaf = NULL;
    return info;
}

/**
 * @brief 从根节点查找目标键所在叶子节点的路径（返回路径栈）
 *
 * 从根向下遍历，每一层将父节点信息（parent/index/leaf）入栈，
 * 直到到达叶子节点。返回的栈顶为最近叶子节点的父节点信息。
 *
 * @param tree 目标树指针
 * @param node 起始节点
 * @param key  目标键
 * @return vector_t* 路径栈（tree_node_info_t* 元素），分配失败返回 NULL
 */
vector_t* b_plus_tree_find_leaf(b_plus_tree_t* tree, b_plus_tree_node_t* node, void* key) {
    vector_t* stack = vector_new();
    while(!node->isLeaf) {
        int i = node->numKeys - 1;
        /* 从右向左找到目标子节点索引 */
        while (i >= 0 && tree->type->cmp(key, node->keys[i]) < 0) {
            i--;
        }
        tree_node_info_t* info = tree_node_info_new();
        if (info == NULL) goto error;
        info->parent = node;
        info->index = i + 1;
        node = node->children[i + 1];
        info->leaf = node;
        vector_push(stack, info);
    }
    return stack;
error:
    /* TODO: 清理已分配的栈元素 */
    return NULL;
}

/**
 * @brief 从叶子节点中删除指定键
 *
 * 找到键后将后续键值前移，numKeys 减一。
 * @param tree 目标树指针
 * @param leaf 目标叶子节点
 * @param key  待删除键
 * @return bool 找到并删除返回 true，未找到返回 false
 */
bool b_plus_tree_leaf_node_remove(b_plus_tree_t* tree, b_plus_tree_node_t* leaf, void* key) {
    int index = 0;
    /* 找到键的位置 */
    while (index < leaf->numKeys && tree->type->cmp(key, leaf->keys[index]) != 0) {
        printf("%d != %d\n", key, leaf->keys[index]);
        index++;
    }
    if (index < leaf->numKeys) {
        /* 前移覆盖删除位置 */
        for (int i = index; i < leaf->numKeys - 1; i++) {
            leaf->keys[i] = leaf->keys[i + 1];
            leaf->data[i] = leaf->data[i + 1];
        }
        leaf->numKeys--;
        return true;
    } else {
        LATTE_LIB_LOG(LOG_ERROR, "leaf[%p] not find key: %p", leaf, key);
        return false;
    }
}

/**
 * @brief 从兄弟节点借一个键（叶子节点再平衡）
 *
 * direction=0（从左兄弟借）：将叶子节点数据右移，从左兄弟尾部借一条记录，更新父节点分隔键。
 * direction=1（从右兄弟借）：将右兄弟首条记录追加到叶子末尾，右兄弟数据前移，更新父节点分隔键。
 *
 * @param tree      目标树指针
 * @param parent    父节点
 * @param index     叶子节点在父节点 children 数组中的索引
 * @param direction 借键方向（0=左兄弟，1=右兄弟）
 * @param leaf      当前叶子节点
 * @param sibling   兄弟节点
 */
void b_plus_tree_borrow_key_from_sibling(b_plus_tree_t* tree, b_plus_tree_node_t* parent, int index, int direction, b_plus_tree_node_t* leaf, b_plus_tree_node_t* sibling) {
    if (direction == 0) {
        /* 从左侧兄弟借：叶子数据右移，借左兄弟最后一条记录 */
        for(int i = leaf->numKeys - 1; i > 0; i--) {
            leaf->keys[i] = leaf->keys[i-1];
            leaf->data[i] = leaf->data[i-1];
        }
        printf("leaf[0] = %d, %d = %d", sibling->keys[sibling->numKeys - 1], parent->keys[index - 1], sibling->keys[sibling->numKeys - 1]);
        leaf->keys[0] = sibling->keys[sibling->numKeys - 1];
        leaf->data[0] = sibling->data[sibling->numKeys - 1];
        /* 更新父节点分隔键 */
        parent->keys[index - 1] = sibling->keys[sibling->numKeys - 1];
        parent->data[index - 1] = sibling->data[sibling->numKeys - 1];
    } else {
        /* 从右侧兄弟借：将右兄弟首条记录追加到叶子末尾 */
        latte_assert_with_info(sibling->numKeys >= 2, "borrow sibling node num_keys < 2");
        leaf->keys[leaf->numKeys] = sibling->keys[0];
        leaf->data[leaf->numKeys] = sibling->data[0];
        printf("leaf[last] = %d, %d = %d", sibling->keys[0], parent->keys[index], sibling->keys[1]);
        /* 更新父节点分隔键为右兄弟新首键 */
        parent->keys[index] = sibling->keys[1];
        parent->data[index] = sibling->data[1];
        /* 右兄弟数据前移 */
        for(int i = 1; i < sibling->numKeys; i++) {
            sibling->keys[i - 1] = sibling->keys[i];
            sibling->data[i - 1] = sibling->data[i];
        }
    }
    leaf->numKeys++;
    sibling->numKeys--;
}

/**
 * @brief 合并两个叶子节点（left 和 right 合并为 left）
 *
 * 将 right 的所有键值追加到 left，更新 left->next 跳过 right，
 * 然后从父节点中删除 index 位置的键和 index+1 位置的 right 子节点指针。
 * @param tree   目标树指针
 * @param parent 父节点
 * @param index  left 节点在父节点 keys 数组中的索引（分隔键索引）
 * @param left   左节点（合并目标）
 * @param right  右节点（被合并后释放）
 */
void b_plus_tree_merge_nodes(b_plus_tree_t* tree, b_plus_tree_node_t* parent, int index, b_plus_tree_node_t* left, b_plus_tree_node_t* right) {
    printf("b_plus_tree_merge_nodes %d %d %d \n", index, left->numKeys, right->numKeys);
    /* 将 right 的键值追加到 left */
    for(int i = 0; i < right->numKeys; i++) {
        left->keys[left->numKeys + i] = right->keys[i];
        left->data[left->numKeys + i] = right->data[i];
    }
    left->numKeys += right->numKeys;
    /* 从父节点删除分隔键 */
    for(int i = index; i < parent->numKeys - 1; i++) {
        parent->keys[i] = parent->keys[i + 1];
        parent->data[i] = parent->data[i + 1];
    }
    /* 从父节点删除 right 子节点指针 */
    for(int i = index + 1; i < parent->numKeys; i++) {
        parent->children[i] = parent->children[i + 1];
    }
    parent->numKeys--;
    left->next = right->next; /* 跳过 right 维护叶子链表 */
    b_plus_tree_node_delete(right);
}

/**
 * @brief 处理叶子节点删除后父节点的空节点情况（递归向上）
 *
 * 当节点 numKeys=0 时，尝试递归向上清理父节点，
 * 若根节点变空则将根置 NULL 或提升其唯一子节点。
 * @param tree  目标树指针
 * @param stack 路径栈（tree_node_info_t* 元素）
 */
void b_plus_tree_move_leaf(b_plus_tree_t* tree, vector_t* stack) {
    if (vector_size(stack) == 0) return;
    tree_node_info_t* leaf_node = vector_get_last(stack);

    if (leaf_node->leaf->isLeaf) {
        /* 叶子节点为空：清理父节点子节点指针，向上递归 */
        if (leaf_node->leaf->numKeys == 0 && leaf_node->parent->numKeys == 0) {
            leaf_node->parent->children[0] = NULL;
            vector_pop(stack);
            b_plus_tree_move_leaf(tree, stack);
            if (leaf_node->parent == tree->root) {
                tree->root = NULL;
            }
        }
    } else {
        /* 内部节点为空：从父节点中移除 */
        if (leaf_node->leaf->numKeys == 0 &&
            leaf_node->leaf->children[0] == NULL) {

            if (leaf_node->parent->numKeys == 0) {
                leaf_node->parent->children[0] = NULL;
                vector_pop(stack);
                b_plus_tree_move_leaf(tree, stack);
                if (leaf_node->parent == tree->root) {
                    tree->root = NULL;
                }
            } else {
                /* 从父节点中移除该空节点 */
                for(int i = leaf_node->index; i < leaf_node->parent->numKeys; i++) {
                    leaf_node->parent->children[i] = leaf_node->parent->children[i + 1];
                }
                leaf_node->parent->numKeys--;
                /* 根节点为空时，提升其唯一子节点为新根 */
                if (leaf_node->parent == tree->root && leaf_node->parent->numKeys == 0) {
                    tree->root = leaf_node->parent->children[0];
                }
            }
        }
    }
}

/**
 * @brief 叶子节点删除后的再平衡调整
 *
 * 按优先级尝试：
 *   1. 从左兄弟借键（左兄弟有富余）
 *   2. 从右兄弟借键（右兄弟有富余）
 *   3. 与左兄弟合并（叶子非首节点）
 *   4. 与右兄弟合并（叶子是首节点）
 *   5. 叶子为空时递归向上移除
 *
 * @param tree  目标树指针
 * @param stack 路径栈（栈顶为当前叶子节点的父节点信息）
 */
void b_plus_tree_adjust_leaf_balance(b_plus_tree_t* tree, vector_t* stack) {
    tree_node_info_t* leaf_node = vector_get_last(stack);
    int index = leaf_node->index;
    b_plus_tree_node_t* parent = leaf_node->parent;
    b_plus_tree_node_t* leaf = leaf_node->leaf;

    if (index > 0 && parent->children[index - 1]->numKeys > tree->order/2) {
        /* 左兄弟有富余：向左借 */
        printf("[b_plus_tree_adjust_leaf_balance] borrow left\n");
        b_plus_tree_borrow_key_from_sibling(tree, parent, index, 0, leaf, parent->children[index - 1]);
    } else if (index < parent->numKeys && parent->children[index + 1]->numKeys > tree->order/2) {
        /* 右兄弟有富余：向右借 */
        printf("[b_plus_tree_adjust_leaf_balance] borrow right\n");
        b_plus_tree_borrow_key_from_sibling(tree, parent, index, 1, leaf, parent->children[index + 1]);
    } else if(parent->numKeys > 0) {
        if (index > 0) {
            /* 叶子非首位：与左兄弟合并 */
            printf("[b_plus_tree_adjust_leaf_balance] merge left\n");
            b_plus_tree_merge_nodes(tree, parent, index - 1, parent->children[index - 1], leaf);
        } else {
            /* 叶子是首位：与右兄弟合并 */
            printf("[b_plus_tree_adjust_leaf_balance] merge right %d\n", parent->numKeys);
            b_plus_tree_merge_nodes(tree, parent, index, leaf, parent->children[index + 1]);
        }
    } else {
        /* 父节点也为空：叶子为空时向上递归清理 */
        if (leaf->numKeys == 0) {
            b_plus_tree_move_leaf(tree, stack);
        }
    }
}

/**
 * @brief 按键删除节点
 *
 * 先通过 b_plus_tree_find_leaf 定位叶子节点路径栈，
 * 从叶子节点中删除键，若叶子节点键数低于最小值则触发再平衡。
 * @param tree 目标树指针
 * @param key  待删除键
 * @return bool 找到并删除返回 true，未找到返回 false
 */
bool b_plus_tree_remove(b_plus_tree_t* tree, void* key) {
    printf("find start \n");
    vector_t* stack = b_plus_tree_find_leaf(tree, tree->root, key);
    if (vector_size(stack) > 0) {
        tree_node_info_t* leaf_node = vector_get_last(stack);
        if (!b_plus_tree_leaf_node_remove(tree, leaf_node->leaf, key)) return false;
        printf("remove end \n");

        /* 叶子键数低于最小值时再平衡 */
        if (leaf_node->leaf->numKeys < tree->order/2) {
            printf("change tree %d\n", tree->root == leaf_node->leaf);
            if (tree->root == leaf_node->leaf) {
                /* 叶子即根节点：直接置 NULL */
                if (leaf_node->leaf->numKeys == 0) {
                    tree->root = NULL;
                    b_plus_tree_node_delete(leaf_node->leaf);
                }
            } else {
                b_plus_tree_adjust_leaf_balance(tree, stack);
            }
        }
    } else {
        /* 无路径栈：叶子就是根节点，直接删除 */
        if (!b_plus_tree_leaf_node_remove(tree, tree->root, key)) return false;
        if (tree->root->numKeys == 0) {
            tree->root = NULL;
        }
    }
    return true;
}

/**
 * @brief 释放整个 B+ 树及所有节点
 * @param tree 目标树指针
 */
void b_plus_tree_delete(b_plus_tree_t* tree) {
    b_plus_tree_node_delete(tree->root);
    zfree(tree);
}

/**
 * @brief B+ 树迭代器 has_next 函数：判断是否还有节点
 * @param it 通用迭代器指针
 * @return bool 当前节点非 NULL 返回 true
 */
bool b_plus_tree_iterator_has_next_func(latte_iterator_t* it) {
    b_plus_tree_iterator_data* data = it->data;
    return data->node != NULL;
}

/**
 * @brief B+ 树迭代器 next 函数：返回当前键值对并推进
 *
 * 读取 node->keys[index] 和 node->data[index] 填入 pair，
 * index 自增；到达当前叶子末尾时通过 next 指针切换到下一叶子节点。
 * @param it 通用迭代器指针
 * @return void* 当前键值对（latte_pair_t*）
 */
void* b_plus_tree_iterator_next_func(latte_iterator_t* it) {
    b_plus_tree_iterator_data* data = it->data;
    data->pair.key = data->node->keys[data->index];
    data->pair.value = data->node->data[data->index];
    data->index++;
    /* 到达叶子末尾：切换到下一叶子节点 */
    if (data->index == data->node->numKeys) {
        data->node = data->node->next;
        data->index = 0;
    }
    return &data->pair;
}

/**
 * @brief 创建 B+ 树迭代器内部数据
 * @param node 起始叶子节点
 * @return b_plus_tree_iterator_data* 新建迭代器数据，失败返回 NULL
 */
b_plus_tree_iterator_data* b_plus_tree_iterator_data_new(b_plus_tree_node_t* node) {
    b_plus_tree_iterator_data* data = zmalloc(sizeof(b_plus_tree_iterator_data));
    if (data == NULL) return data;
    data->node = node;
    data->index = 0;
    data->pair.key = NULL;
    data->pair.value = NULL;
    return data;
}

/**
 * @brief 释放 B+ 树迭代器内部数据
 * @param data 迭代器内部数据指针
 */
void b_plus_tree_iterator_data_delete(b_plus_tree_iterator_data* data) {
    zfree(data);
}

/**
 * @brief B+ 树迭代器 release 函数：释放迭代器内存
 * @param it 通用迭代器指针
 */
void b_plus_tree_iterator_delete_func(latte_iterator_t* it) {
    b_plus_tree_iterator_data_delete(it->data);
    zfree(it);
}

/** @brief B+ 树迭代器虚函数表 */
latte_iterator_func bPlusIteratorType = {
    .has_next = b_plus_tree_iterator_has_next_func,
    .next = b_plus_tree_iterator_next_func,
    .release = b_plus_tree_iterator_delete_func,
};

/**
 * @brief 获取 B+ 树的顺序迭代器（从最小键叶子节点开始遍历）
 *
 * 从根沿 children[0] 找到最左叶子节点作为迭代起点，
 * 通过叶子节点的 next 链表实现顺序遍历。
 * @param tree 目标树指针
 * @return latte_iterator_t* 新建迭代器（调用方负责释放），失败返回 NULL
 */
latte_iterator_t* b_plus_tree_get_iterator(b_plus_tree_t* tree) {
    latte_iterator_t* it = zmalloc(sizeof(latte_iterator_t));
    if (it == NULL) return NULL;
    it->func = &bPlusIteratorType;
    b_plus_tree_node_t* node = tree->root;
    if (node != NULL) {
        /* 沿最左路径找到第一个叶子节点 */
        while (!node->isLeaf) {
            node = node->children[0];
        }
    }
    b_plus_tree_iterator_data* data = b_plus_tree_iterator_data_new(node);
    if (data == NULL) goto error;
    it->data = data;
    return (latte_iterator_t*)it;
error:
    zfree(it);
    return NULL;
}
