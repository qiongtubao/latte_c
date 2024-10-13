#include "avlSet.h"
#include "sds/sds.h"


int avlSetAdd(avlSet* set, void* key) {
    return avlTreePut(set, key, NULL);
}

int avlSetContains(avlSet* set, void* element) {
    avlNode* node = avlTreeGet(set, element);
    return node == NULL? 0 : 1;
}

int avlSetTypeAdd(set_t *set, void* key) {
    return avlSetAdd(set->data, key);
}

int avlSetTypeContains(set_t *set, void* key) {
    return avlSetContains(set->data, key);
}

int avlSetTypeRemove(set_t* set, void* key) {
    return avlSetRemove(set->data, key);
}

size_t avlSetTypeSize(set_t* set) {
    return avlSetSize(set->data);
}

void avlSetTypeRelease(set_t* set) {
    avlSetRelease(set->data);
    zfree(set);
}

latte_iterator_t* avlSetTypeGetIterator(set_t* set) {
    return avlSetGetIterator(set->data);
}

struct set_func_t avlSetApi = {
    .add = avlSetTypeAdd,
    .contains = avlSetTypeContains,
    .remove = avlSetTypeRemove,
    .size = avlSetTypeSize,
    .release = avlSetTypeRelease,
    .getIterator = avlSetTypeGetIterator
};
set_t* set_newAvl(avlTreeType* type) {
    set_t* s = zmalloc(sizeof(set_t));
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
    sds_delete(node->key);
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

