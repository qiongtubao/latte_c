#include "avlSet.h"
#include "sds/sds.h"




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

struct setType avlSetApi = {
    .add = avlSetTypeAdd,
    .contains = avlSetTypeContains,
    .remove = avlSetTypeRemove,
    .size = avlSetTypeSize,
    .release = avlSetTypeRelease,
};
set* setCreateAvl(avlTreeType* type) {
    set* s = zmalloc(sizeof(set));
    s->data = avlSetCreate(&type);
    s->type = &avlSetApi;
    return s;
}
avlNode* createSetSdsNode(void* key, void* value) {
    avlNode* node = zmalloc(sizeof(avlNode));
    node->left = NULL;
    node->right = NULL;
    node->key = sdsdup(key);
    node->height = 0;
    return node;
}

void UNSETVAL(avlNode* node, void* val) {
    
}

void releaseSetSdsNode(avlNode* node) {
    sdsfree(node->key);
    zfree(node);
}
avlTreeType avlSetSdsType = {
    .createNode = createSetSdsNode,
    .nodeSetVal = UNSETVAL,
    .operator = sdscmp,
    .releaseNode = releaseSetSdsNode
};

