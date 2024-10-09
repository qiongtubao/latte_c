
#ifndef __LATTE_AVL_TREE_H
#define __LATTE_AVL_TREE_H
#include "stdlib.h"
#include "iterator/iterator.h"
#include "stdbool.h"
/**
 * avl 
 */
typedef struct avlNode avlNode;
typedef void* (*getvalue_func_t)(const void*);
typedef struct avlTreeType {
    int (*operator)(void* f1, void* f2);
    void (*releaseNode)(avlNode* node);
    avlNode* (*createNode)(void* key, void* value);
    getvalue_func_t getValue;
    void (*nodeSetVal)(avlNode* node, void* value);
} avlTreeType;

typedef struct avlNode {
    void* key;
    struct avlNode* left;
    struct avlNode* right;
    int height;
    // void* value; // 值可以是任意类型，使用 void* 通用指针
} avlNode;

typedef struct avlTreeIterator {
    avlNode* current;
    avlNode** stack;
    size_t stackSize;
    size_t top;
} avlTreeIterator;

typedef struct avlTree {
    avlTreeType* type;
    avlNode* root;
} avlTree;

avlTree* avlTreeCreate(avlTreeType* type);
void avlTreeRelease(avlTree* tree);

int avlTreePut(avlTree* tree, void* key, void* value);
void* avlTreeGet(avlTree* tree, void* key);
avlNode* avlTreeGetMin(avlTree* tree);
int avlTreeRemove(avlTree* tree, void* key);
void avlTreeRelease(avlTree* tree);
int avlTreeSize(avlTree* tree);


avlTreeIterator* avlTreeGetAvlTreeIterator(avlTree* tree);
bool avlTreeIteratorHasNext(avlTreeIterator* iterator);
avlNode* avlTreeIteratorNext(avlTreeIterator* iterator);
void avlTreeIteratorRelease(avlTreeIterator* iterator);

typedef struct avlTreeLatteIterator {
    Iterator iterator;
    keyValuePair pair;
    avlTree* tree;
} avlTreeLatteIterator;

Iterator* avlTreeGetIterator(avlTree* tree);



#endif