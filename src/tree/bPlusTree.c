#include "bPlusTree.h"
#include <stdbool.h>
#include <stdlib.h>
#include "zmalloc/zmalloc.h"
#include "utils/utils.h"
#include <string.h>
#include "log/log.h"

bPlusTreeRoot* bPlusTreeRootCreate(bPlusTreeType* type, int order) {
    bPlusTreeRoot* root = zmalloc(sizeof(bPlusTreeRoot));
    if (root == NULL) return NULL;
    root->root = bPlusTreeNodeCreate(order, true); //空节点
    root->type = type;
    root->order = order;
    return root;
}




bPlusTreeNode* bPlusTreeNodeCreate(int order, int isLeaf) {
    bPlusTreeNode* node = zmalloc(sizeof(bPlusTreeNode));
    node->isLeaf = isLeaf;
    node->numKeys = 0;
    node->keys = zmalloc(sizeof(void*) * (order - 1));
    node->data = zmalloc(sizeof(void*) * (order - 1));
    node->children = zmalloc(sizeof(bPlusTreeNode*) * order);
    return node;
}


void bPlusTreeNodeRelease(bPlusTreeNode* node) {
    if (node == NULL) return;

    for (int i = 0; i < node->numKeys; i++) {
        bPlusTreeNodeRelease(node->children[i]);
    }
    zfree(node->keys);
    zfree(node->data);
    zfree(node->children);
    zfree(node);
}
void insertNonFull(bPlusTreeNode* node, void* key, void* data, cmp_func_t cmp) {
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

void splitChild(bPlusTreeRoot* tree, bPlusTreeNode* parent, int index, void* key, void* data) {
    bPlusTreeNode* child = parent->children[index];
    bPlusTreeNode* newChild = bPlusTreeNodeCreate(tree->order, child->isLeaf);
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
    parent->numKeys++;

    if (tree->type->cmp(key, child->keys[t - 1]) > 0) {
        insertNonFull(newChild, key, data, tree->type->cmp);
    } else {
        insertNonFull(child, key, data, tree->type->cmp);
    }
}

void bPlusTreeInsert(bPlusTreeRoot* tree, void* key, void* data) {
    bPlusTreeNode *node = tree->root;
    if (node->numKeys == tree->order - 1) {
        bPlusTreeNode * newRoot = bPlusTreeNodeCreate(tree->order, false);
        newRoot->children[0] = node;
        splitChild(tree, newRoot, 0, key, data);
        tree->root = newRoot;
    } else {
        insertNonFull(node, key, data, tree->type->cmp);
    }
}



void** bPlusTreeFind_(bPlusTreeRoot* tree, bPlusTreeNode* node, void* key) {
    int i = 0;
    while (i < node->numKeys && tree->type->cmp(key, node->keys[i]) > 0) {
        i++;
    }
    if (node->isLeaf) {
        if (i < node->numKeys && tree->type->cmp(key, node->keys[i]) == 0) {
            return &node->data[i];
        }
        return NULL;
    }
    return bPlusTreeFind_(tree, node->children[i], key);
}
void* bPlusTreeFind(bPlusTreeRoot* tree, void* key) {
    void** resultPoint = bPlusTreeFind_(tree, tree->root, key);
    if (resultPoint == NULL) return NULL;
    return *resultPoint;
}

void bPlusTreeUpdate(bPlusTreeRoot* tree, void* key, void* data) {
    void** oldData = bPlusTreeFind_(tree, tree->root, key);
    latte_assert(oldData != NULL , "bplus tree update find fail!");
    if (tree->type->freeNode != NULL) tree->type->freeNode(*oldData);
    *oldData = data;
}

void mergeWithSibling(bPlusTreeRoot *tree, bPlusTreeNode *node, int i) {
    bPlusTreeNode *leftSibling = node->children[i - 1];
    bPlusTreeNode *rightSibling = node->children[i];

    // Merge right sibling into left sibling
    leftSibling->keys[leftSibling->numKeys] = node->keys[i - 1];
    leftSibling->data[leftSibling->numKeys] = node->data[i - 1];
    for (int j = 0; j < rightSibling->numKeys; j++) {
        leftSibling->keys[leftSibling->numKeys + 1 + j] = rightSibling->keys[j];
        leftSibling->data[leftSibling->numKeys + 1 + j] = rightSibling->data[j];
    }
    for (int j = 0; j <= rightSibling->numKeys; j++) {
        leftSibling->children[leftSibling->numKeys + 1 + j] = rightSibling->children[j];
    }
    leftSibling->numKeys += rightSibling->numKeys + 1;

    // Remove merged sibling from parent
    memmove(node->keys + i - 1, node->keys + i, sizeof(void *) * (node->numKeys - i));
    memmove(node->data + i - 1, node->data + i, sizeof(void *) * (node->numKeys - i));
    memmove(node->children + i - 1, node->children + i + 1, sizeof(bPlusTreeNode *) * (node->numKeys - i));
    node->numKeys--;
    
    // Free the merged sibling
    bPlusTreeNodeRelease(rightSibling);
}

void redistributeFromRightSibling(bPlusTreeRoot *tree, bPlusTreeNode *node, int i) {
    bPlusTreeNode *leftSibling = node->children[i];
    bPlusTreeNode *rightSibling = node->children[i + 1];

    // Move one key from parent to left sibling and adjust pointers
    leftSibling->keys[leftSibling->numKeys] = node->keys[i];
    leftSibling->data[leftSibling->numKeys] = node->data[i];
    leftSibling->children[leftSibling->numKeys + 1] = rightSibling->children[0];
    leftSibling->numKeys++;

    // Move parent's key and adjust pointers
    memmove(node->keys + i, node->keys + i + 1, sizeof(void *) * (node->numKeys - i - 1));
    memmove(node->data + i, node->data + i + 1, sizeof(void *) * (node->numKeys - i - 1));
    memmove(node->children + i + 1, node->children + i + 2, sizeof(bPlusTreeNode *) * (node->numKeys - i - 1));
    node->numKeys--;
}

void redistributeFromLeftSibling(bPlusTreeRoot *tree, bPlusTreeNode *node, int i) {
    bPlusTreeNode *leftSibling = node->children[i - 1];
    bPlusTreeNode *rightSibling = node->children[i];

    // Move one key from parent to right sibling and adjust pointers
    rightSibling->keys[0] = node->keys[i - 1];
    rightSibling->data[0] = node->data[i - 1];
    rightSibling->children[0] = leftSibling->children[leftSibling->numKeys];
    rightSibling->numKeys++;

    // Move parent's key and adjust pointers
    memmove(node->keys + i - 1, node->keys + i, sizeof(void *) * (node->numKeys - i));
    memmove(node->data + i - 1, node->data + i, sizeof(void *) * (node->numKeys - i));
    memmove(node->children + i, node->children + i + 1, sizeof(bPlusTreeNode *) * (node->numKeys - i));
    node->numKeys--;
}

void deleteKey(bPlusTreeRoot *tree, bPlusTreeNode **node_, void *key) {
    bPlusTreeNode* node = *node_;
    int minDegree = tree->order / 2;

    if (node->isLeaf) {
        int i = 0;
        while (i < node->numKeys && tree->type->cmp(key, node->keys[i]) > 0) {
            i++;
        }
        if (i < node->numKeys && tree->type->cmp(key, node->keys[i]) == 0) {
            memmove(node->keys + i, node->keys + i + 1, sizeof(void *) * (node->numKeys - i - 1));
            memmove(node->data + i, node->data + i + 1, sizeof(void *) * (node->numKeys - i - 1));
            node->numKeys--;
        } else {
            // Key not found in leaf node
            return;
        }
    } else {
        int i = 0;
        while (i < node->numKeys && tree->type->cmp(key, node->keys[i]) > 0) {
            i++;
        }
        
        // If the child node has fewer than the minimum degree of keys, handle it
        if (node->children[i]->numKeys < minDegree) {
            // Try to redistribute or merge with sibling
            if (i > 0 && node->children[i - 1]->numKeys >= minDegree) {
                // Borrow from left sibling
                redistributeFromLeftSibling(tree, node, i);
            } else if (i < node->numKeys && node->children[i + 1]->numKeys >= minDegree) {
                // Borrow from right sibling
                redistributeFromRightSibling(tree, node, i);
            } else {
                // Merge with sibling
                mergeWithSibling(tree, node, i);
            }
        }

        // Recursively delete from the child node
        deleteKey(tree, &node->children[i], key);
    }

    // Handle root shrinkage
    if (node->numKeys == 0 && node != tree->root) {
        *node_ = node->children[0];
        bPlusTreeNodeRelease(node);
    }
}

// Helper functions







void bPlusTreeDelete(bPlusTreeRoot* tree, void* key) {
    deleteKey(tree, &(tree->root), key);
}

bool bPlusIteratorHasNext(latte_iterator_t* it_) {
    bPlusTreeIterator* it = (bPlusTreeIterator*)it_;
    bPlusTreeNode* currentNode = it->iterator.data;
    if (currentNode == NULL || it->currentIndex >= currentNode->numKeys) {
        return false;
    }
    return true;
}

void* bPlusIteratorNext(latte_iterator_t* it_) {
    bPlusTreeIterator* it = (bPlusTreeIterator*)it_;
    bPlusTreeNode* currentNode = it->iterator.data;
    if (currentNode == NULL || it->currentIndex >= currentNode->numKeys) {
        return NULL;
    }

    it->pair.key= currentNode->keys[it->currentIndex];
    it->pair.value = currentNode->data[it->currentIndex];
    it->currentIndex++;

    if (it->currentIndex >= currentNode->numKeys) {
        if (currentNode->children != NULL && currentNode->children[it->currentIndex] != NULL) {
            it->currentIndex = 0;
            it->iterator.data = currentNode->children[it->currentIndex];
        } else {
            it->iterator.data = NULL;
        }
    }
    return &it->pair;
}

void bPlusIteratorRelease(latte_iterator_t* it) {
    zfree(it);
}
latte_iterator_func bPlusIteratorType = {
    .has_next = bPlusIteratorHasNext,
    .next = bPlusIteratorNext,
    .release = bPlusIteratorRelease,
};

latte_iterator_t* bPlusTreeGetIterator(bPlusTreeRoot* tree) {
    bPlusTreeIterator* it = zmalloc(sizeof(bPlusTreeIterator));
    if (it == NULL) return NULL;
    it->iterator.func = &bPlusIteratorType;
    it->currentIndex = 0;
    bPlusTreeNode* node = tree->root;
    while (!node->isLeaf) {
        node = node->children[0];
    }
    it->iterator.data = node;
    return (latte_iterator_t*)it;
}