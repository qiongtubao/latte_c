


#ifndef __LATTE_RB_TREE_H
#define __LATTE_RB_TREE_H

#include "tree.h"
#include <stdlib.h>
#include <stdbool.h>
#include "iterator/iterator.h"
typedef enum rbColor { 
    RED, 
    BLACK 
} rbColor;

typedef struct rbNode {
    void* key;
    rbColor color;
    struct rbNode *left;
    struct rbNode *right;
    struct rbNode *parent;
} rbNode;




typedef struct rbTree {
    rbNode *root;
    treeType* type;
} rbTree;

typedef struct rbIterator {
    rbNode *current;
    rbTree *tree;
} rbIterator;

rbTree* rbTreeCreate(treeType* type);
int rbTreePut(rbTree* tree, void* key, void* value);
rbNode* rbTreeGetNode(rbTree* tree, void* key);
rbIterator* rbTreeGetIterator(rbTree* tree);
bool rbIteratorHasNext(rbIterator* iterator);
rbNode* rbIteratorNext(rbIterator* iterator);
void rbIteratorRelease(rbIterator* iterator);



#endif