#include "avlSet.h"
#include "sds/sds.h"


int avlSetAdd(avlSet* set, void* key) {
    return avlTreePut(set, key, NULL);
}

int avlSetContains(avlSet* set, void* element) {
    avlNode* node = avlTreeGet(set, element);
    return node == NULL? 0 : 1;
}

int avlSetTypeAdd(set *set, void* key) {
    return avlSetAdd(set->data, key);
}

int avlSetTypeContains(set *set, void* key) {
    return avlSetContains(set->data, key);
}

int avlSetTypeRemove(set* set, void* key) {
    return avlSetRemove(set->data, key);
}

size_t avlSetTypeSize(set* set) {
    return avlSetSize(set->data);
}

void avlSetTypeRelease(set* set) {
    avlSetRelease(set->data);
    zfree(set);
}

Iterator* avlSetTypeGetIterator(set* set) {
    return avlSetGetIterator(set->data);
}

struct setType avlSetApi = {
    .add = avlSetTypeAdd,
    .contains = avlSetTypeContains,
    .remove = avlSetTypeRemove,
    .size = avlSetTypeSize,
    .release = avlSetTypeRelease,
    .getIterator = avlSetTypeGetIterator
};
set* setCreateAvl(avlTreeType* type) {
    set* s = zmalloc(sizeof(set));
    s->data = avlSetCreate(type);
    s->type = &avlSetApi;
    return s;
}
avlNode* createSetSdsNode(void* key, void* value) {
    avlNode* node = zmalloc(sizeof(avlNode));
    node->left = NULL;
    node->right = NULL;
    node->key = sds_dup(key);
    node->height = 0;
    return node;
}

void UNSETVAL(avlNode* node, void* val) {
    
}

void releaseSetSdsNode(avlNode* node) {
    sds_free(node->key);
    zfree(node);
}

int avlSetSdsOperator(void* a, void* b) {
    return sds_cmp((sds)a, (sds)b);
}
avlTreeType avlSetSdsType = {
    .createNode = createSetSdsNode,
    .nodeSetVal = UNSETVAL,
    .operator = avlSetSdsOperator,
    .releaseNode = releaseSetSdsNode
};

