#include "rbTree.h"
#include "zmalloc/zmalloc.h"
#include <stdlib.h>



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
    // rbNode* y = NULL;
    // rbNode* x = tree->root;

    // while (x != NULL) {
    //     y = x;
    //     int operator = tree->type->operator(key, x->key);
    //     if (operator > 0) {
    //         x = x->left;
    //     } else if(operator < 0) {
    //         x = x->right;
    //     } else {
    //         tree->type->setVal(x, value);
    //         *action = 0;
    //         return x;
    //     }
    // }
    // rbNode *new_node = tree->type->createNode(key, value);
    // new_node->parent = y;
    // new_node->color = RED;
    
    // *action = 1;
    // if (y == NULL) {
    //     tree->root = new_node;
    //     new_node->color = BLACK;
    // } else {
    //     int operator = tree->type->operator(key, y->key);
    //     if (operator > 0) {
    //         y->left = new_node;
    //     } else {
    //         y->right = new_node;
    //     }
    // }
    
    // rbTreeInsertFixup(tree, new_node);
    // return new_node;
    rbNode *new_node = tree->type->createNode(key, value);
    new_node->color = RED;
    rbNode *y = NULL;
    rbNode *x = tree->root;
    *action = 1;

    while (x != NULL) {
        y = x;
        int op = tree->type->operator(new_node->key, x->key);
        if (op > 0) {
            x = x->left;
        } else if (op < 0) {
            x = x->right;
        } else {
            *action = 0;
            tree->type->setVal(x, value);
            return x;
        }
    }
    new_node->parent = y;
    if (y == NULL) {
        tree->root = new_node;
        new_node->color = BLACK; // Root must be black
    } else {
        int op = tree->type->operator(new_node->key, y->key);
        if (op > 0) {
            y->left = new_node;
        } else {
            printf("right\n");
            y->right = new_node;
        }
    }
    rbTreeInsertFixup(tree, new_node);
    return new_node;
}

int rbTreePut(rbTree* tree, void* key, void* value) {
    int puted = -1;
    rbTreeInsert(tree, key, value, &puted);
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

rbIterator* rbTreeGetIterator(rbTree* tree) {
    rbIterator* iterator = zmalloc(sizeof(rbIterator));
    iterator->current = NULL;
    iterator->tree = tree;
    return iterator;
}
bool rbIteratorHasNext(rbIterator* iter) {
    if (iter->current == NULL) {
        if (iter->tree->root != NULL) {
            return true;
        } else {
            return false;
        }
    }
    rbIterator i = {
        .current = iter->current
    };
    if (i.current->right != NULL) {
        return true;
    } else {
        rbNode *parent = i.current->parent;
        while (parent != NULL && i.current == parent->right) {
            i.current = parent;
            parent = parent->parent;
        }
        if (parent == NULL) {
            return false;
        }
        return true;
    }
}
rbNode* rbIteratorNext(rbIterator* iter) {
    if (iter->current == NULL) {
        if (iter->tree->root == NULL) {
            return NULL;
        } else {
            iter->current = iter->tree->root;
            while (iter->current->left != NULL) {
                iter->current = iter->current->left;
            }
            return iter->current;
        }
    }

    if (iter->current->right != NULL) {
        iter->current = iter->current->right;
        while (iter->current->left != NULL) {
            iter->current = iter->current->left;
        }
        return iter->current;
    } else {
        rbNode *parent = iter->current->parent;
        while (parent != NULL && iter->current == parent->right) {
            iter->current = parent;
            parent = parent->parent;
        }
        if (parent == NULL) {
            iter->current = NULL;
            return NULL;
        }
        iter->current = parent;
        return iter->current;
    }
}
void rbIteratorRelease(rbIterator* iter) {
    zfree(iter);
}