#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <unistd.h>
#include "avlTree.h"
#include "zmalloc/zmalloc.h"
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
int avlKeyOperator(void* key, void* key1) {
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
    .operator = avlKeyOperator,
    .releaseNode = avlNodeRelease
};
int test_tree() {
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
        avlNode* node = avlTreeIteratorNext(avlTreeIterator);
        assert(node->key == result[i]);
        i++;
    }
    assert(i == 4);
    avlTreeIteratorRelease(avlTreeIterator);

    Iterator* iterator = avlTreeGetIterator(tree);
    i = 0;
    while(iteratorHasNext(iterator)) {
        avlNode* node = (avlNode*)iteratorNext(iterator);
        assert(node->key == result[i]);
        i++;
    }
    assert(i == 4);
    iteratorRelease(iterator);

    assert(avlTreeRemove(tree, 10L) == 1);
    assert(avlTreeGet(tree, 10L) == NULL);
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("about tree function", 
            test_tree() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}