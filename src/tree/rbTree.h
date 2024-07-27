


#ifndef __LATTE_RB_TREE_H
#define __LATTE_RB_TREE_H

#include "tree.h"
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

rbTree* rbTreeCreate(treeType* type);
rbNode* rbNodeCreate(void* key, void* value);
int rbTreePut(rbTree* tree, void* key, void* value);
rbNode* rbTreeGetNode(rbTree* tree, void* key);
#endif