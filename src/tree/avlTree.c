#include "avlTree.h"
#include "zmalloc/zmalloc.h"

avlTree* avlTreeCreate(avlTreeType* type) {
    avlTree* tree = zmalloc(sizeof(avlTree));
    tree->type = type;
    tree->root = NULL;
    return tree;
}

void avlNodeRecursively(avlNode** node, avlTreeType* type) {
    if (*node != NULL) {
        avlNodeRecursively(&(*node)->left, type);
        avlNodeRecursively(&(*node)->right, type);
        type->releaseNode(*node);
        *node = NULL;
    }
}

void avlTreeRelease(avlTree* tree) {
    avlNodeRecursively(&tree->root, tree->type);
    zfree(tree);
}

int avlNodeGetHeight(avlNode* node) {
    if (node == NULL) {
        return 0;
    }
    return node->height;
}
#define max(a,b) a < b? b: a

int getBalance(avlNode* node) {
    if (node == NULL) {
        return 0;
    }
    return avlNodeGetHeight(node->left) - avlNodeGetHeight(node->right);
}

avlNode* rotateRight(avlNode* y) {
    avlNode* x = y->left;
    avlNode* T2 = x->right;

    // Perform rotation
    x->right = y;
    y->left = T2;

    // Update heights
    y->height = max(avlNodeGetHeight(y->left), avlNodeGetHeight(y->right)) + 1;
    x->height = max(avlNodeGetHeight(x->left), avlNodeGetHeight(x->right)) + 1;

    return x;
}

avlNode* rotateLeft(avlNode* x) {
    avlNode* y = x->right;
    avlNode* T2 = y->left;

    // Perform rotation
    y->left = x;
    x->right = T2;

    // Update heights
    x->height = max(avlNodeGetHeight(x->left), avlNodeGetHeight(x->right)) + 1;
    y->height = max(avlNodeGetHeight(y->left), avlNodeGetHeight(y->right)) + 1;

    return y;
}

avlNode* avlNodeInsert(avlNode* node, avlTreeType* type, void* key, void* value, int* puted) {
    if (node == NULL) {
        avlNode* node = type->createNode(key, value);
        node->left = node->right = NULL;
        node->height = 1;
        *puted = 1;
        return node;
    }
    int cmp = type->operator(key, node->key);
    if (cmp < 0) {
        node->left = avlNodeInsert(node->left, type, key, value, puted);
    } else if (cmp > 0) {
        node->right = avlNodeInsert(node->right, type, key, value, puted);
    } else {
        type->nodeSetVal(node, value);
        *puted = 0;
    }

    node->height = 1 + max(avlNodeGetHeight(node->left), avlNodeGetHeight(node->right));

    int balance = getBalance(node);
    if (balance > 1 && type->operator(key, node->left->key) > 0) {
        return rotateRight(node);
    }

    if (balance < -1 && type->operator(key, node->right->key) < 0) {
        return rotateLeft(node);
    }

    if (balance > 1 && type->operator(key, node->left->key) < 0) {
        node->left = rotateLeft(node->left);
        return rotateRight(node);
    }

    if (balance < -1 && type->operator(key, node->right->key) > 0) {
        node->right = rotateRight(node->right);
        return rotateLeft(node);
    }
    return node;
} 

/**
 *  -1 添加失败
 *  0  更新
 *  1  添加
 */
int avlTreePut(avlTree* tree, void* key, void* value) {
    int puted = -1;
    tree->root = avlNodeInsert(tree->root, tree->type, key, value, &puted);
    return puted;
}

void* avlNodeSearch(avlNode* node, avlTreeType* type, void* key) {
    if (node == NULL) {
        return NULL;
    }
    int result = type->operator(node->key,  key);
    if (result == 0) return node;
    if (result < 0) {
        return avlNodeSearch(node->left, type, key);
    }
    return avlNodeSearch(node->right, type, key);
}

void* avlTreeGet(avlTree* tree, void* key) {
    return avlNodeSearch(tree->root, tree->type, key);
}

avlNode* minValueNode(avlNode* node) {
    avlNode* current = node;
    while (current && current->left != NULL) {
        current = current->left;
    }
    return current;
}

avlNode* avlTreeGetMin(avlTree* tree) {
    return minValueNode(tree->root);
}

avlNode* deleteNode(avlNode* node, avlTreeType* type, void* key, int* deleted) {
    if (node == NULL) return node;

    int result = type->operator(key, node->key);
    if (result > 0) {
        node->left = deleteNode(node->left, type, key, deleted);
    } else if (result < 0) {
        node->right = deleteNode(node->right, type, key, deleted);
    } else {
        if ((node ->left == NULL)  || (node->right == NULL)) {
            avlNode* temp = node->left ? node->left: node->right;
            if (temp == NULL) {
                temp = node;
                node = NULL;
            } else {
                *node = *temp;
            }
            *deleted = 1;
            type->releaseNode(temp);
        } else {
            avlNode* temp = minValueNode(node->right);
            node->key = temp->key;
            node->right = deleteNode(node->right, type, temp->key, deleted);
        }
    }

    if (node == NULL) return node;

    node->height = 1 + max(avlNodeGetHeight(node->left), avlNodeGetHeight(node->right));

    int balance = getBalance(node);

    if (balance > 1 && getBalance(node->left) >= 0) {
        return rotateRight(node);
    }

    if (balance > 1 && getBalance(node->left) < 0) {
        node->left = rotateLeft(node->left);
        return rotateRight(node);
    }

    if (balance < -1 && getBalance(node->right) <= 0) {
        return rotateLeft(node);
    }

    if (balance < -1 && getBalance(node->right) > 0) {
        node->right = rotateRight(node->right);
        return rotateLeft(node);
    }
    return node;
}

int avlTreeRemove(avlTree* tree, void* key) {
    int delete = 0;
    tree->root = deleteNode(tree->root, tree->type, key, &delete);
    return delete;
}



int countNodes(avlNode* node) {
    if (node == NULL) {
        return 0;
    }
    return 1 + countNodes(node->left) + countNodes(node->right);
}
int avlTreeSize(avlTree* tree) {
    return countNodes(tree->root);
}


avlTreeIterator* avlTreeGetAvlTreeIterator(avlTree* tree) {
    avlTreeIterator* it = zmalloc(sizeof(avlTreeIterator));
    it->current = tree->root;
    it->stackSize = 16; //初始栈大小
    it->stack = zmalloc(it->stackSize * sizeof(avlNode*));
    it->top = 0;

    //将所有左孩子入栈
    while (it->current != NULL) {
        it->stack[it->top++] = it->current;
        if (it->top == it->stackSize) {
            it->stackSize *= 2;
            it->stack = zrealloc(it->stack, it->stackSize * sizeof(avlNode*));
        }
        it->current = it->current->left;
    }
    return it;
}

bool avlTreeIteratorHasNext(avlTreeIterator* iterator) {
    return iterator->top > 0;
}
avlNode* avlTreeIteratorNext(avlTreeIterator* iterator) {
    if (iterator->top <= 0) {
        return NULL;
    }
    avlNode* node = iterator->stack[--iterator->top];
    iterator->current = node->right;
    while(iterator->current != NULL) {
        iterator->stack[iterator->top++] = iterator->current;
        if (iterator->top == iterator->stackSize) {
            iterator->stackSize *= 2;
            iterator->stack = zrealloc(iterator->stack, iterator->stackSize * sizeof(avlNode*));
        }
        iterator->current = iterator->current->left;
    }
    return node;
}

void avlTreeIteratorRelease(avlTreeIterator* iterator) {
    zfree(iterator->stack);
    zfree(iterator);
}

void* avlTreeIteratorTypeNext(Iterator* iterator) {
   return avlTreeIteratorNext(iterator->data);
}

void avlTreeIteratorTypeRelease(Iterator* iterator) {
    avlTreeIteratorRelease(iterator->data);
    zfree(iterator);
}

bool avlTreeIteratorTypeHasNext(Iterator* iterator) {
    return avlTreeIteratorHasNext(iterator->data);
}

IteratorType avlTreeIteratorType = {
     .next = avlTreeIteratorTypeNext,
     .release = avlTreeIteratorTypeRelease,
     .hasNext = avlTreeIteratorTypeHasNext
};
Iterator* avlTreeGetIterator(avlTree* tree) {
    Iterator* iterator = zmalloc(sizeof(Iterator));
    iterator->type = &avlTreeIteratorType;
    iterator->data = avlTreeGetAvlTreeIterator(tree);
    return iterator;
}