#ifndef __LATTE_B_PLUS_TREE_H
#define __LATTE_B_PLUS_TREE_H

#include "iterator/iterator.h"
/**
 * B+树的特点
    1. 所有叶子节点都在同一层。
    2. 非叶子节点可以包含指向子节点的指针和关键字。
    3. 叶子节点包含关键字和相应的数据记录地址或数据本身。
    4. 非叶子节点用于引导搜索。
    5. 每个节点最多包含M个子节点（M是树的阶）。
    6. 每个节点至少包含ceil(M/2)个子节点（除了根节点）。
 */

typedef struct bPlusTreeNode {
    int isLeaf;
    int numKeys;
    void** keys; // 存储关键字  void* keys[order - 1] 
    void** data; // 存储数据记录或指向数据记录的指针 void* data[order - 1] 
    struct bPlusTreeNode **children; // 存储子节点指针 void* children[order] 
} bPlusTreeNode;
bPlusTreeNode* bPlusTreeNodeCreate(int order, int isLeaf);
void bPlusTreeNodeRelease(bPlusTreeNode* node);

typedef int (*cmp_func_t)(void*, void*);
typedef struct bPlusTreeType {
    cmp_func_t cmp;
    void (*freeNode)(void* value);
} bPlusTreeType;

typedef struct bPlusTreeRoot {
    bPlusTreeType* type;
    bPlusTreeNode* root;
    int order;
} bPlusTreeRoot;
bPlusTreeRoot* bPlusTreeRootCreate(bPlusTreeType* type, int order);
void bPlusTreeInsert(bPlusTreeRoot* tree, void* key, void* data);
void* bPlusTreeFind(bPlusTreeRoot* tree, void* key);
void bPlusTreeUpdate(bPlusTreeRoot* tree, void* key, void* data);
void bPlusTreeDelete(bPlusTreeRoot* tree, void* key);

typedef struct bPlusTreeIterator {
    latte_iterator_t iterator;              //data is bPlusTreeNode
    int currentIndex;               //当前叶子节点上的索引
    latte_pair_t pair;
} bPlusTreeIterator;

latte_iterator_t* bPlusTreeGetIterator(bPlusTreeRoot* tree);

#endif