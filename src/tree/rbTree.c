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

rbIterator* rbTreeGetRbIterator(rbTree* tree) {
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

bool rbTreeIteratorHasNext(Iterator *iterator) {
    rbIterator * rbIter = iterator->data;
    return rbIteratorHasNext(rbIter);
}

void* rbTreeIteratorNext(Iterator *iterator) {
    rbIterator * rbIter = iterator->data;
    return rbIteratorNext(rbIter);
}

void rbTreeIteratorRelease(Iterator *iterator) {
    rbIteratorRelease(iterator->data);
    zfree(iterator);
}

IteratorType rbTreeIteratorType = {
    .hasNext = rbTreeIteratorHasNext,
    .next = rbTreeIteratorNext,
    .release = rbTreeIteratorRelease
};

Iterator* rbTreeGetIterator(rbTree* tree) {
    Iterator* iterator = zmalloc(sizeof(Iterator));
    iterator->data = rbTreeGetRbIterator(tree);
    iterator->type = &rbTreeIteratorType;
    return iterator;
}

void rbTransplant(rbTree *tree, rbNode *u, rbNode *v) {
    if (u->parent == NULL) {
        tree->root = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }
    v->parent = u->parent;
}

rbNode *rbTreeMinimum(rbNode *node) {
    while (node->left != NULL) {
        node = node->left;
    }
    return node;
}

void rbDeleteFixup(rbTree *tree, rbNode *node) {
    while (node != tree->root && node->color == BLACK) {
        if (node == node->parent->left) {
            rbNode *sibling = node->parent->right;
            if (sibling->color == RED) {
                sibling->color = BLACK;
                node->parent->color = RED;
                rbTreeLeftRotate(tree, node->parent);
                sibling = node->parent->right;
            }
            if ((sibling->left == NULL || sibling->left->color == BLACK)
                && (sibling->right == NULL || sibling->right->color == BLACK)) {
                sibling->color = RED;
                node = node->parent;
            } else {
                if (sibling->right == NULL || sibling->right->color == BLACK) {
                    if (sibling->left != NULL) sibling->left->color = BLACK;
                    sibling->color = RED;
                    rbTreeRightRotate(tree, sibling);
                    sibling = node->parent->right;
                }
                sibling->color = node->parent->color;
                node->parent->color = BLACK;
                if (sibling->right != NULL) sibling->right->color = BLACK;
                rbTreeLeftRotate(tree, node->parent);
                node = tree->root;
            }
        } else { // node is the right child
            rbNode *sibling = node->parent->left;
            if (sibling->color == RED) {
                sibling->color = BLACK;
                node->parent->color = RED;
                rbTreeRightRotate(tree, node->parent);
                sibling = node->parent->left;
            }
            if ((sibling->right == NULL || sibling->right->color == BLACK)
                && (sibling->left == NULL || sibling->left->color == BLACK)) {
                sibling->color = RED;
                node = node->parent;
            } else {
                if (sibling->left == NULL || sibling->left->color == BLACK) {
                    if (sibling->right != NULL) sibling->right->color = BLACK;
                    sibling->color = RED;
                    rbTreeLeftRotate(tree, sibling);
                    sibling = node->parent->left;
                }
                sibling->color = node->parent->color;
                node->parent->color = BLACK;
                if (sibling->left != NULL) sibling->left->color = BLACK;
                rbTreeRightRotate(tree, node->parent);
                node = tree->root;
            }
        }
    }
    node->color = BLACK;
}

int rbTreeRemove(rbTree* tree, void* key) {
    rbNode *z = rbTreeGetNode(tree, key);
    if (z == NULL) return 0;

    rbNode *y = z;
    rbColor y_original_color = y->color;
    rbNode *x;

    if (z->left == NULL) {
        x = z->right;
        rbTransplant(tree, z, z->right);
    } else if (z->right == NULL) {
        x = z->left;
        rbTransplant(tree, z, z->left);
    } else {
        y = rbTreeMinimum(z->right);
        y_original_color = y->color;
        x = y->right;
        if (y->parent == z) {
            x->parent = y;
        } else {
            rbTransplant(tree, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }
        rbTransplant(tree, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->color = z->color;
    }
    tree->type->releaseNode(z);

    if (y_original_color == BLACK) {
        rbDeleteFixup(tree, x);
    }
    return 1;
}