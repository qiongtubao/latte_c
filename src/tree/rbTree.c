#include "rbTree.h"
#include "zmalloc/zmalloc.h"
#include <stdlib.h>

rbNode* rbNodeCreate(void* key, void* value) {
    rbNode* node = zmalloc(sizeof(rbNode));
    node->key = key;
    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;
    node->color = RED;
    return node;
}

void rbTreeLeftRotate(rbTree* tree, rbNode* node) {
    rbNode *y = node->right;
    node->right = y->left;
    if (y->left != NULL) y->left->parent = node;
    y->parent = node->parent;
    if (node->parent == NULL) tree->root = y;
    else if (node == node->parent->left) node->parent->left = y;
    else node->parent->right = y;
    y->left = node;
    node->parent = y;
}

void rbTreeRightRotate(rbTree *tree, rbNode* node) {
    rbNode *y = node->left;
    node->left = y->right;
    if (y->right != NULL) y->right->parent = node;
    y->parent = node->parent;
    if (node->parent == NULL) tree->root = y;
    else if (node == node->parent->right) node->parent->right = y;
    else node->parent->left = y;
    y->right = node;
    node->parent = y;
}

void rbTreeInsertFixup(rbTree* tree, rbNode* node) {
    while (node->parent != NULL && node->parent->color == RED) {
        if (node->parent == node->parent->parent->left) {
            rbNode *uncle = node->parent->parent->right;
            if (uncle != NULL && uncle->color == RED) {
                node->parent->color = BLACK;
                uncle->color = BLACK;
                node->parent->parent->color = RED;
                node = node->parent->parent;
            } else {
                if (node == node->parent->right) {
                    node = node->parent;
                    rbTreeLeftRotate(tree, node);
                }
                node->parent->color = BLACK;
                node->parent->parent->color = RED;
                rbTreeRightRotate(tree, node->parent->parent);
            }
        } else {
            rbNode *uncle = node->parent->parent->left;
            if (uncle != NULL && uncle->color == RED) {
                node->parent->color = BLACK;
                uncle->color = BLACK;
                node->parent->parent->color = RED;
                node = node->parent->parent;
            } else {
                if (node == node->parent->left) {
                    node = node->parent;
                    rbTreeRightRotate(tree, node);
                }
                node->parent->color = BLACK;
                node->parent->parent->color = RED;
                rbTreeLeftRotate(tree, node->parent->parent);
            }
        }
    }
    tree->root->color = BLACK;

}

rbTree* rbTreeCreate(treeType* type) {
    rbTree* tree = zmalloc(sizeof(rbTree));
    tree->type = type;
    tree->root = NULL;
    return tree;
}

rbNode* rbTreeInsert(rbTree* tree, void* key, void* value, int* action) {
    rbNode* y = NULL;
    rbNode* x = tree->root;

    while (x != NULL) {
        y = x;
        int operator = tree->type->operator(key, x->key);
        if (operator > 0) {
            x = x->left;
        } else if(operator < 0) {
            x = x->right;
        } else {
            tree->type->setVal(x, value);
            *action = 0;
            return x;
        }
    }
    rbNode *new_node = tree->type->createNode(key, value);
    new_node->parent = y;
    new_node->color = RED;
    
    *action = 1;
    if (y == NULL) {
        tree->root = new_node;
    } else {
        int operator = tree->type->operator(key, y->key);
        if (operator > 0) {
            y->left = new_node;
        } else {
            y->right = new_node;
        }
    }
    
    rbTreeInsertFixup(tree, new_node);
    return new_node;
}

int rbTreePut(rbTree* tree, void* key, void* value) {
    int puted = -1;
    tree->root = rbTreeInsert(tree, key, value, &puted);
    return puted;
}

rbNode* rbTreeGetNode(rbTree* tree, void* key) {
    rbNode* node = tree->root;
    while (node != NULL) {
        int opt = tree->type->operator(key, node->key);
        if (opt > 0) {
            node = node->left;
        } else if (opt < 0) {
            node = node->right;
        } else {
            return node;
        }
    }
    return NULL;
}