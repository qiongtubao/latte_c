#include "b_plus_tree.h"
#include <stdbool.h>
#include <stdlib.h>
#include "zmalloc/zmalloc.h"
#include "utils/utils.h"
#include <string.h>
#include "log/log.h"

b_plus_tree_t* b_plus_tree_new(b_plus_tree_func_t* type, int order) {
    b_plus_tree_t* root = zmalloc(sizeof(b_plus_tree_t));
    if (root == NULL) return NULL;
    root->root = NULL;
    root->type = type;
    root->order = order;
    return root;
}




b_plus_tree_node_t* b_plus_tree_node_new(int order, int isLeaf) {
    latte_assert_with_info(order > 1, "b_plus_tree_node_new order <= 1");
    b_plus_tree_node_t* node = zmalloc(sizeof(b_plus_tree_node_t));
    node->isLeaf = isLeaf;
    node->numKeys = 0;
    node->keys = zmalloc(sizeof(void*) * (order));
    node->data = zmalloc(sizeof(void*) * (order));
    node->children = zmalloc(sizeof(b_plus_tree_node_t*) * (order + 1));
    for(int i = 0; i < order;i++) {
        node->keys[i] = NULL;
        node->data[i] = NULL;
        node->children[i] = NULL;
    }
    node->children[order] = NULL;
    node->next = NULL;
    return node;
}


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
void b_plus_tree_node_insert_non_full(b_plus_tree_node_t* node, void* key, void* data, cmp_func cmp) {
    int i = node->numKeys - 1;
    if (!node->isLeaf) {
        while( i >= 0 && cmp(key , node->keys[i]) < 0) {
            i--;
        }
        i++;
        node->children[i + 1] = node->children[i];
    }
    while (i >= 0 && cmp(key , node->keys[i]) < 0) {
        i--;
    }
    i++;
    for (int j = node->numKeys; j > i; j--) {
        if (!node->isLeaf) {
            //移动children
            node->children[j] = node->children[j - 1];
        }
        node->keys[j] = node->keys[j - 1];
        node->data[j] = node->data[j - 1];
    }
    if (!node->isLeaf) {
        node->children[i] = node->children[i - 1];
    }
    LATTE_LIB_LOG(LOG_DEBUG, "insert key: %p, value: %p ,index: %d", key,data, i);
    node->keys[i] = key;
    node->data[i] = data;
    node->numKeys++;
}

void b_plus_tree_split_child(b_plus_tree_t* tree, b_plus_tree_node_t* parent, int index, void* key, void* data) {
    b_plus_tree_node_t* child = parent->children[index];
    b_plus_tree_node_t* newChild = b_plus_tree_node_new(tree->order, child->isLeaf);
    int t = tree->order / 2;
    for (int j = 0; j < t - 1;j++) {
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

    for (int i = parent->numKeys - 1; i >= index + 1; i--) {
        parent->children[i + 1] = parent->children[i];
    }
    parent->children[index + 1] = newChild;

    parent->keys[parent->numKeys] = child->keys[t - 1];
    parent->data[parent->numKeys] = child->data[t - 1];
    parent->numKeys++;

    if (tree->type->cmp(key, child->keys[t - 1]) > 0) {
        b_plus_tree_node_insert_non_full(newChild, key, data, tree->type->cmp);
    } else {
        b_plus_tree_node_insert_non_full(child, key, data, tree->type->cmp);
    }
}



/**
 * @brief 
 * 
 * @param parent_node 
 * @param tree 
 * @param child_node 
 *          把child_node 数据移动到新的node上 并且设置好index
 * @param index 
 */
void b_plus_tree_node_split(b_plus_tree_node_t* parent_node, b_plus_tree_t *tree, b_plus_tree_node_t* child_node, int index) {
    b_plus_tree_node_t *new_node = b_plus_tree_node_new(tree->order, child_node->isLeaf);
    //奇数的话  3个移动1个到新节点
    //老节点多  新节点少 
    if (child_node->isLeaf) {
        //ord = 2 move 1   (1)
        //ord = 3 move 1   (2)
        //ord = 4 move 2   (2,3)
        for(int i = 0; i < tree->order/2; i++) {
            new_node->keys[i] = child_node->keys[i + (tree->order+ 1) / 2 ];
            new_node->data[i] = child_node->data[i + (tree->order+ 1) / 2 ];
        }
        //ord = 2  (1 + 1)
        //ord = 3  (2 + 1)
        //ord = 4  (2 + 2)
        child_node->numKeys = (tree->order+ 1)/2;
        new_node->numKeys = (tree->order)/2;
        new_node->next = child_node->next;
        child_node->next = new_node;
    } else {
        //ord = 2 move 1 (2)
        //ord = 3 move 2 (2)
        //ord = 4 move 2 (3)
        for(int i = 0; i < (tree->order+1) / 2; i++) {
            //ord = 2  [0] = [2]
            //ord = 3  [0] = [2]
            //ord = 4  [0] = [3]
             new_node->children[i] = child_node->children[i + (tree->order)/ 2 + 1];
        }
        // ord = 2 move 0 (*)
        // ord = 3 move 1 (2)
        // ord = 4 move 1 (3)
        for(int i = 0; i < (tree->order-1)/2; i++) {
            //ord = 2  
            //ord = 3  [0] = [2]
            //ord = 4  [0] = [3]
            new_node->keys[i] = child_node->keys[i + (tree->order) / 2 + 1 ];
            new_node->data[i] = child_node->data[i + (tree->order) / 2 + 1 ];
        }
        //ord = 2  keys = 1 + 0
        //ord = 3  keys = 1 + 1 
        // ord = 4 keys = 2 + 1
        child_node->numKeys = (tree->order)/2;
        new_node->numKeys = (tree->order-1)/2;
    }

    for (int i = parent_node->numKeys; i > index; i--) {
        printf("split move child node set key %d = %d\n", i, parent_node->keys[i - 1]);
        parent_node->keys[i] = parent_node->keys[i - 1];
        parent_node->data[i] = parent_node->data[i - 1];
        parent_node->children[i + 1] = parent_node->children[i];
    }

    // //把index + 1 到最后的数据往后移动, 空出index + 1 位置;
    // for (int i = parent_node->numKeys + 1; i > index + 1; i--) {
        
    // }
    parent_node->children[index + 1] = new_node;
    latte_assert_with_info(child_node->numKeys  != 0 , "child_node numkeys != 0\n");
    //ord = 2 (1)
    //ord = 3 (1)
    //ord = 4 (2)
    parent_node->keys[index] = child_node->keys[tree->order/2];
    parent_node->data[index] = child_node->data[tree->order/2];
    parent_node->numKeys++;
}

// 插入键值
void b_plus_tree_node_insert(b_plus_tree_node_t* node, b_plus_tree_t *tree, void* key, void* data) {
    int i = node->numKeys - 1;

    if (node->isLeaf) {
        //是叶子结点
        printf("[insert leaf node] move start %d \n", i);
        //key < node->keys[i]
        //向后移动数据
        while (i >= 0 && tree->type->cmp(key, node->keys[i]) < 0) {
            printf("[insert leaf node] set %d = %d \n", i+1, node->keys[i]);
            node->keys[i + 1] = node->keys[i];
            node->data[i + 1] = node->data[i];
            i--;
        }
        printf("[insert leaf node] move end %d , set %d = %d\n", i , i 
        + 1, key);
        //添加新数据
        node->keys[i + 1] = key;
        node->data[i + 1] = data;
        node->numKeys++;
    } else {
        //非叶子节点
        // key < node->keys[i]
        printf("[insert no leaf node] find start %d %d\n", i, node->keys[i]);
        //查找到第几个节点
        while (i >= 0 && tree->type->cmp(key, node->keys[i]) < 0 ) {
            i--;
        }
        i++;
        printf("[insert no leaf node] find %p %d %p %d %d %d\n", node, i, node->children[i], node->numKeys, key, node->keys[i-1]);
        //如果子节点满了
        if (node->children[i]->numKeys == tree->order) {
            printf("[insert no leaf node] split %d \n", i);
            b_plus_tree_node_split(node, tree, node->children[i], i);
            //key > node->keys[i]
            if (tree->type->cmp(key, node->keys[i]) > 0) {
                i++;
            }
        }
        printf("[insert no leaf node] insert %d \n", i);
        b_plus_tree_node_insert(node->children[i], tree, key, data);
    }
}
void b_plus_tree_insert(b_plus_tree_t* tree, void* key, void* data) {
    if (tree->root == NULL) {
        //开始既是root节点又是叶子节点
        tree->root = b_plus_tree_node_new(tree->order, true);
        b_plus_tree_node_insert(tree->root, tree, key, data);
    } else {
        if (tree->root->numKeys == tree->order) {
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


void** private_b_plus_tree_find(b_plus_tree_t* tree, b_plus_tree_node_t* node, void* key) {
    int i = node->numKeys - 1;
    while (i >= 0 && tree->type->cmp(key, node->keys[i]) < 0 ) {
        i--;
    }
    if (node->isLeaf) {
        if (i < node->numKeys && tree->type->cmp(key, node->keys[i]) == 0) {
            return &node->data[i];
        }
        return NULL;
    } else {
        i++;
    }
    return private_b_plus_tree_find(tree, node->children[i], key);
}
void* b_plus_tree_find(b_plus_tree_t* tree, void* key) {
    void** resultPoint = private_b_plus_tree_find(tree, tree->root, key);
    if (resultPoint == NULL) return NULL;
    return *resultPoint;
}

void b_plus_tree_update(b_plus_tree_t* tree, void* key, void* data) {
    latte_assert_with_info(tree->root != NULL, "tree not data");
    void** oldData = private_b_plus_tree_find(tree, tree->root, key);
    latte_assert_with_info(oldData != NULL , "bplus tree update find fail!");
    if (tree->type->free_node != NULL) tree->type->free_node(*oldData);
    *oldData = data;
}

void b_plus_tree_insert_or_update(b_plus_tree_t* tree, void* key, void* data) {
    if (tree->root != NULL) {
        void** oldData = private_b_plus_tree_find(tree, tree->root, key);
        if (oldData != NULL) {
            if (tree->type->free_node != NULL) tree->type->free_node(*oldData);
            *oldData = data;
        } else {
            b_plus_tree_insert(tree, key, data);
        }
    } else {
        b_plus_tree_insert(tree, key, data);
    }
    
}

// void b_plus_tree_merge_with_sibling(b_plus_tree_t *tree, b_plus_tree_node_t *node, int i) {
//     b_plus_tree_node_t *leftSibling = node->children[i - 1];
//     b_plus_tree_node_t *rightSibling = node->children[i];

//     // Merge right sibling into left sibling
//     leftSibling->keys[leftSibling->numKeys] = node->keys[i - 1];
//     leftSibling->data[leftSibling->numKeys] = node->data[i - 1];
//     for (int j = 0; j < rightSibling->numKeys; j++) {
//         leftSibling->keys[leftSibling->numKeys + 1 + j] = rightSibling->keys[j];
//         leftSibling->data[leftSibling->numKeys + 1 + j] = rightSibling->data[j];
//     }
//     for (int j = 0; j <= rightSibling->numKeys; j++) {
//         leftSibling->children[leftSibling->numKeys + 1 + j] = rightSibling->children[j];
//     }
//     leftSibling->numKeys += rightSibling->numKeys + 1;

//     // Remove merged sibling from parent
//     memmove(node->keys + i - 1, node->keys + i, sizeof(void *) * (node->numKeys - i));
//     memmove(node->data + i - 1, node->data + i, sizeof(void *) * (node->numKeys - i));
//     memmove(node->children + i - 1, node->children + i + 1, sizeof(b_plus_tree_node_t *) * (node->numKeys - i));
//     node->numKeys--;
    
//     // Free the merged sibling
//     b_plus_tree_node_delete(rightSibling);
// }

// void b_plus_tree_redistribute_from_right_sibling(b_plus_tree_t *tree, b_plus_tree_node_t *node, int i) {
//     b_plus_tree_node_t *leftSibling = node->children[i];
//     b_plus_tree_node_t *rightSibling = node->children[i + 1];

//     // Move one key from parent to left sibling and adjust pointers
//     leftSibling->keys[leftSibling->numKeys] = node->keys[i];
//     leftSibling->data[leftSibling->numKeys] = node->data[i];
//     leftSibling->children[leftSibling->numKeys + 1] = rightSibling->children[0];
//     leftSibling->numKeys++;

//     // Move parent's key and adjust pointers
//     memmove(node->keys + i, node->keys + i + 1, sizeof(void *) * (node->numKeys - i - 1));
//     memmove(node->data + i, node->data + i + 1, sizeof(void *) * (node->numKeys - i - 1));
//     memmove(node->children + i + 1, node->children + i + 2, sizeof(b_plus_tree_node_t *) * (node->numKeys - i - 1));
//     node->numKeys--;
// }

// void b_plus_tree_redistribute_from_left_sibling(b_plus_tree_t *tree, b_plus_tree_node_t *node, int i) {
//     (void)tree;
//     b_plus_tree_node_t *leftSibling = node->children[i - 1];
//     b_plus_tree_node_t *rightSibling = node->children[i];

//     // Move one key from parent to right sibling and adjust pointers
//     rightSibling->keys[0] = node->keys[i - 1];
//     rightSibling->data[0] = node->data[i - 1];
//     rightSibling->children[0] = leftSibling->children[leftSibling->numKeys];
//     rightSibling->numKeys++;

//     // Move parent's key and adjust pointers
//     memmove(node->keys + i - 1, node->keys + i, sizeof(void *) * (node->numKeys - i));
//     memmove(node->data + i - 1, node->data + i, sizeof(void *) * (node->numKeys - i));
//     memmove(node->children + i, node->children + i + 1, sizeof(b_plus_tree_node_t *) * (node->numKeys - i));
//     node->numKeys--;
// }

// void b_plus_tree_remove_key(b_plus_tree_t *tree, b_plus_tree_node_t **node_, void *key) {
//     b_plus_tree_node_t* node = *node_;
//     int minDegree = tree->order / 2;
//     int i = node->numKeys - 1;
//     while (i >= 0 && tree->type->cmp(key, node->keys[i]) < 0 ) {
//         i--;
//     }
//     if (node->isLeaf) {
//         if (i < node->numKeys && tree->type->cmp(key, node->keys[i]) == 0) {
//             memmove(node->keys + i, node->keys + i + 1, sizeof(void *) * (node->numKeys - i - 1));
//             memmove(node->data + i, node->data + i + 1, sizeof(void *) * (node->numKeys - i - 1));
//             node->numKeys--;
//         } else {
//             // Key not found in leaf node
//             return;
//         }
//     } else {
//         i++;
//         // If the child node has fewer than the minimum degree of keys, handle it
//         if (node->children[i]->numKeys < minDegree) {
//             // Try to redistribute or merge with sibling
//             if (i > 0 && node->children[i - 1]->numKeys >= minDegree) {
//                 // Borrow from left sibling
//                 b_plus_tree_redistribute_from_left_sibling(tree, node, i);
//             } else if (i < node->numKeys && node->children[i + 1]->numKeys >= minDegree) {
//                 // Borrow from right sibling
//                 b_plus_tree_redistribute_from_right_sibling(tree, node, i);
//             } else {
//                 // Merge with sibling
//                 b_plus_tree_merge_with_sibling(tree, node, i);
//             }
//         }

//         // Recursively delete from the child node
//         b_plus_tree_remove_key(tree, &node->children[i], key);
//     }

//     // Handle root shrinkage
//     if (node->numKeys == 0 && node != tree->root) {
//         *node_ = node->children[0];
//         b_plus_tree_node_delete(node);
//     }
// }

// Helper functions



tree_node_info_t* tree_node_info_new() {
    tree_node_info_t* info = zmalloc(sizeof(tree_node_info_t));
    if (info == NULL) return NULL;
    info->index = 0;
    info->parent = NULL;
    info->leaf = NULL;
    return info;
}

//查找到可能所在的节点
vector_t* b_plus_tree_find_leaf(b_plus_tree_t* tree, b_plus_tree_node_t* node, void* key) {
    //init find_leaf_info
    
    vector_t* stack = vector_new();
    while(!node->isLeaf) {
        int i = node->numKeys - 1;
        while (i >= 0 && tree->type->cmp(key, node->keys[i]) < 0 ) {
            i--;
        }
        tree_node_info_t* info =  tree_node_info_new();
        if (info == NULL) goto error;
        info->parent = node;
        info->index = i+1;
        node = node->children[i+1];
        info->leaf = node;
        vector_push(stack, info);
    }
    return stack;
error:
    //TODO clean stack
    return NULL;
}

//从叶子节点中删除键
bool b_plus_tree_leaf_node_remove(b_plus_tree_t* tree, b_plus_tree_node_t* leaf, void* key) {
    int index = 0;
    while (index < leaf->numKeys && tree->type->cmp(key, leaf->keys[index]) != 0 ) {
        printf("%d != %d\n",key, leaf->keys[index]);
        index++;
    }
    if (index < leaf->numKeys) {
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
 * direction  0 表示左侧兄弟节点
 *            1 表示右侧兄弟节点
 */
void b_plus_tree_borrow_key_from_sibling(b_plus_tree_t* tree, b_plus_tree_node_t* parent, int index, int direction, b_plus_tree_node_t* leaf, b_plus_tree_node_t* sibling) {
    if (direction == 0) { //左侧兄弟节点借数据
        for(int i = leaf->numKeys - 1; i > 0;i--) {
            leaf->keys[i] = leaf->keys[i-1];
            leaf->data[i] = leaf->data[i-1];
        }
        printf("leaf[0] = %d, %d = %d", sibling->keys[sibling->numKeys - 1], parent->keys[index - 1], sibling->keys[sibling->numKeys - 1]);
        
        leaf->keys[0] = sibling->keys[sibling->numKeys - 1];
        leaf->data[0] = sibling->data[sibling->numKeys - 1];
        parent->keys[index - 1] =  sibling->keys[sibling->numKeys - 1];
        parent->data[index - 1] =  sibling->data[sibling->numKeys - 1];
    } else { 
        latte_assert_with_info(sibling->numKeys >=2 , "borrow sibling node num_keys < 2");
        leaf->keys[leaf->numKeys] = sibling->keys[0];
        leaf->data[leaf->numKeys] = sibling->data[0];
        printf("leaf[last] = %d, %d = %d", sibling->keys[0],parent->keys[index],sibling->keys[1]);
        parent->keys[index] = sibling->keys[1];
        parent->data[index] = sibling->data[1];
        for(int i = 1; i < sibling->numKeys;i++) {
            sibling->keys[i - 1] = sibling->keys[i];
            sibling->data[i - 1] = sibling->data[i];
        }
    }
    leaf->numKeys++;
    sibling->numKeys--;
}

void b_plus_tree_merge_nodes(b_plus_tree_t* tree, b_plus_tree_node_t* parent, int index, b_plus_tree_node_t* left, b_plus_tree_node_t* right) {
    printf("b_plus_tree_merge_nodes %d %d %d \n", index,  left->numKeys, right->numKeys);
    for(int i = 0; i < right->numKeys;i++) {
        left->keys[left->numKeys + i] = right->keys[i];
        left->data[left->numKeys + i] = right->data[i]; 
    }
    left->numKeys += right->numKeys;
    for(int i = index; i < parent->numKeys - 1; i++) {
        parent->keys[i] = parent->keys[i+1];
        parent->data[i] = parent->data[i+1];
    }
    for(int i = index + 1; i < parent->numKeys; i++) {
        parent->children[i] = parent->children[i + 1];
    }
    parent->numKeys--;
    left->next = right->next;
    b_plus_tree_node_delete(right);
}

void b_plus_tree_move_leaf(b_plus_tree_t* tree, vector_t* stack) {
    if (vector_size(stack) == 0) return;
    tree_node_info_t* leaf_node = vector_get_last(stack);
   
    if (leaf_node->leaf->isLeaf) {
        if (leaf_node->leaf->numKeys == 0 && leaf_node->parent->numKeys == 0) {
            leaf_node->parent->children[0] = NULL;
            vector_pop(stack);
            b_plus_tree_move_leaf(tree, stack);
            if (leaf_node->parent == tree->root) {
                tree->root = NULL;
            }
        }
    } else {
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
                for(int i = leaf_node->index; i < leaf_node->parent->numKeys; i++) {
                    leaf_node->parent->children[i] = leaf_node->parent->children[i+1];
                }
                leaf_node->parent->numKeys--;
                if (leaf_node->parent == tree->root && leaf_node->parent->numKeys == 0) {
                    tree->root = leaf_node->parent->children[0];
                }
            }
        }
    }
    
}

void b_plus_tree_adjust_leaf_balance(b_plus_tree_t* tree, vector_t* stack) {
    tree_node_info_t* leaf_node =  vector_get_last(stack);
    int index = leaf_node->index;
    b_plus_tree_node_t* parent = leaf_node->parent;
    b_plus_tree_node_t* leaf = leaf_node->leaf;
    if (index > 0 && parent->children[index - 1]->numKeys > tree->order/2) {
        printf("[b_plus_tree_adjust_leaf_balance] borrow left\n");
        //左侧节点 数据 > MIN_KEYS(order/2) 向左侧借数据
        b_plus_tree_borrow_key_from_sibling(tree, parent, index, 0,  leaf, parent->children[index - 1]);
    } else if (index < parent->numKeys && parent->children[index + 1]->numKeys > tree->order/2) {
        printf("[b_plus_tree_adjust_leaf_balance] borrow right\n");
        //向右侧借数据
        b_plus_tree_borrow_key_from_sibling(tree, parent, index, 1,  leaf, parent->children[index + 1]);
    } else if(parent->numKeys > 0) {
        if (index > 0) {//叶子是最后一个节点 和前面的节点合并
            printf("[b_plus_tree_adjust_leaf_balance] merge left\n");
            b_plus_tree_merge_nodes(tree, parent, index - 1, parent->children[index - 1], leaf);
        } else {
            printf("[b_plus_tree_adjust_leaf_balance] merge right %d\n", parent->numKeys);
            b_plus_tree_merge_nodes(tree, parent, index, leaf, parent->children[index + 1]);
        }
    } else {
        
        if (leaf->numKeys == 0) {
            b_plus_tree_move_leaf(tree, stack);
        }
    }
    
}   

bool b_plus_tree_remove(b_plus_tree_t* tree, void* key) {
    // b_plus_tree_remove_key(tree, &(tree->root), key);
    printf("find start \n");
    vector_t* stack =  b_plus_tree_find_leaf(tree, tree->root, key);
    if (vector_size(stack) > 0) {
        tree_node_info_t* leaf_node = vector_get_last(stack);
        if (!b_plus_tree_leaf_node_remove(tree, leaf_node->leaf, key)) return false;
        printf("remove end \n");

        if (leaf_node->leaf->numKeys < tree->order/2) {
            printf("change tree %d\n", tree->root == leaf_node->leaf);
            //如果是树节点的话
            if (tree->root == leaf_node->leaf) {
                if (leaf_node->leaf->numKeys == 0) 
                {
                    tree->root = NULL;
                    b_plus_tree_node_delete(leaf_node->leaf);
                }
            } else {
                b_plus_tree_adjust_leaf_balance(tree, stack);
            }
        }
    } else {
                    
        if (!b_plus_tree_leaf_node_remove(tree, tree->root, key)) return false;
        if (tree->root->numKeys == 0) {
            tree->root = NULL;
        }
    }
    return true;
     
    
    
}

void b_plus_tree_delete(b_plus_tree_t* tree) {
    b_plus_tree_node_delete(tree->root);
    zfree(tree);
}




bool b_plus_tree_iterator_has_next_func(latte_iterator_t* it) {
    b_plus_tree_iterator_data* data = it->data;
    return data->node != NULL;
}

void* b_plus_tree_iterator_next_func(latte_iterator_t* it) {
    b_plus_tree_iterator_data* data = it->data;
    data->pair.key = data->node->keys[data->index];
    data->pair.value = data->node->data[data->index];
    data->index++;
    if (data->index == data->node->numKeys ) {
        data->node = data->node->next;
        data->index = 0;
    }
    return &data->pair;
}

b_plus_tree_iterator_data* b_plus_tree_iterator_data_new(b_plus_tree_node_t* node) {
    b_plus_tree_iterator_data* data = zmalloc(sizeof(b_plus_tree_iterator_data));
    if (data == NULL) return data;
    data->node = node;
    data->index =  0;
    data->pair.key = NULL;
    data->pair.value = NULL;
    return data;
}

void b_plus_tree_iterator_data_delete(b_plus_tree_iterator_data* data) {
    zfree(data);
}

void b_plus_tree_iterator_delete_func(latte_iterator_t* it) {
    b_plus_tree_iterator_data_delete(it->data);
    zfree(it);
}
latte_iterator_func bPlusIteratorType = {
    .has_next = b_plus_tree_iterator_has_next_func,
    .next = b_plus_tree_iterator_next_func,
    .release = b_plus_tree_iterator_delete_func,
};











latte_iterator_t* b_plus_tree_get_iterator(b_plus_tree_t* tree) {
    latte_iterator_t* it = zmalloc(sizeof(latte_iterator_t));
    if (it == NULL) return NULL;
    it->func = &bPlusIteratorType;
    b_plus_tree_node_t* node = tree->root;
    if (node != NULL) {
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