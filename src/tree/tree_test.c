#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <unistd.h>
#include "avlTree.h"
#include "bPlusTree.h"
#include "zmalloc/zmalloc.h"
#include "log/log.h"
typedef struct avlIntNode {
    avlNode node;
    void* value;
} avlIntNode;
avlNode* avlTreeCreateIntNode(void* key, void* value) {
    avlIntNode* node = zmalloc(sizeof(avlIntNode));
    node->node.key = key;
    node->value = value;
    return node;
}

void avlNodeSetVal(avlNode* node_, void* value) {
    avlIntNode* node = (avlIntNode*)node_;
    if (node->value != NULL) zfree(node->value);
    node->value = value;
}
int KeyOperator(void* key, void* key1) {
    long k = key;
    long k2 = key1;
    long r = k - k2;
    if (r > 0) return 1;
    if (r < 0) return -1;
    return 0;
}

void avlNodeRelease(avlNode* node_) {
    avlIntNode* node = node_;
    zfree(node->value);
    zfree(node);
}
avlTreeType treeType = {
    .createNode = avlTreeCreateIntNode,
    .nodeSetVal = avlNodeSetVal,
    .operator = KeyOperator,
    .releaseNode = avlNodeRelease,
    .getValue = NULL
};
int test_avl_tree() {
    avlTree* tree = avlTreeCreate(&treeType);
    assert(avlTreeGet(tree, 10L) == NULL);
    assert(avlTreePut(tree, 10L, NULL) == 1);
    assert(avlTreeGet(tree, 10L) != NULL);
    assert(avlTreePut(tree, 10L, NULL) == 0);
    assert(avlTreeGet(tree, 10L) != NULL);
    assert(avlTreePut(tree, 1L, NULL) == 1);
    assert(avlTreePut(tree, 5L, NULL) == 1);
    assert(avlTreePut(tree, 15L, NULL) == 1);

   

    avlTreeIterator* avlTreeIterator = avlTreeGetAvlTreeIterator(tree);
    int result[4] = {1L,5L,10L,15L};
    int i = 0;
    while(avlTreeIteratorHasNext(avlTreeIterator)) {
        keyValuePair* node = avlTreeIteratorNext(avlTreeIterator);
        assert(node->key == result[i]);
        i++;
    }
    assert(i == 4);
    avlTreeIteratorRelease(avlTreeIterator);

    Iterator* iterator = avlTreeGetIterator(tree);
    i = 0;
    while(iteratorHasNext(iterator)) {
        keyValuePair* node = iteratorNext(iterator);
        assert(node->key == result[i]);
        i++;
    }
    assert(i == 4);
    iteratorRelease(iterator);

    assert(avlTreeRemove(tree, 10L) == 1);
    assert(avlTreeGet(tree, 10L) == NULL);
    return 1;
}

// =====

bPlusTreeType btype = {
    .cmp = KeyOperator,
    .freeNode = NULL
};
int test_bplus_tree() {
    bPlusTreeRoot* tree = bPlusTreeRootCreate(&btype, 16);
    bPlusTreeInsert(tree, (void*)10L, (void*)1L);
    bPlusTreeInsert(tree, (void*)3L, (void*)2L);
    bPlusTreeInsert(tree, (void*)20L, (void*)3L);
    bPlusTreeInsert(tree, (void*)5L, (void*)4L);
    bPlusTreeInsert(tree, (void*)15L, (void*)5L);
    bPlusTreeInsert(tree, (void*)1L, (void*)6L);
    bPlusTreeInsert(tree, (void*)21L, (void*)7L);

    Iterator* it =  bPlusTreeGetIterator(tree);
    int list[7] = {1,3,5,10,15,20,21};
    int i = 0;
    while(iteratorHasNext(it)) {
        keyValuePair* pair = iteratorNext(it);
        assert(pair->key == list[i++]);
    }
    iteratorRelease(it);


    void* result = bPlusTreeFind(tree, (void*)10L);
    assert(result == 1L);
    bPlusTreeUpdate(tree, (void*)10L, (void*)2L);
    result = bPlusTreeFind(tree, (void*)10L);
    assert(result == 2L);
    bPlusTreeDelete(tree, (void*)10L);
    result = bPlusTreeFind(tree, (void*)10L);
    assert(result == NULL);
    result = bPlusTreeFind(tree, (void*)15L);
    assert(result == 5L);


    return 1;
}

int test_api(void) {
    log_init();
    assert(log_add_stdout(LATTE_LIB, LOG_DEBUG) == 1);
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("about avl tree function", 
            test_avl_tree() == 1);
        test_cond("about bplus tree function", 
            test_bplus_tree() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}