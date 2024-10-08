#include "hashSet.h"
#include "zmalloc/zmalloc.h"

/* hashSet dictionary type. Keys are SDS strings, values are ot used. */
hashSetType sdsHashSetDictType = {
    dictSdsHash,               /* hash function */
    dictSdsDup,                      /* key dup */
    NULL,                      /* val dup */
    dictSdsKeyCompare,         /* key compare */
    dictSdsDestructor,         /* key destructor */
    NULL                       /* val destructor */
};


hashSet* hashSetCreate(hashSetType* dt) {
    hashSet* s = zmalloc(sizeof(hashSet));
    hashSetInit(s, dt);
    return s;
}

void hashSetRelease(hashSet* hashSet) {
    dictRelease(hashSet);
}

void hashSetInit(hashSet* s, dictType* dt) {
    _dictInit(s, dt);
}
void hashSetDestroy(hashSet* hashSet) {
    dictDestroy(hashSet);
}

/**
 *  return 
 *  0 表示原来已经有了
 *  1 表示新加
 */
int hashSetAdd(hashSet* hashSet, void* element) {
    dictEntry *de = dictAddRaw(hashSet,element,NULL);
    if (de) {
        dictSetVal(hashSet,de,NULL);
        return 1;
    }
    return 0;
}
/**
 *  1 删除成功
 *  0 删除失败
 */
int hashSetRemove(hashSet* hashSet, void* element) {
    if (dictDelete(hashSet ,element) == DICT_OK) {
        if (htNeedsResize(hashSet)) dictResize(hashSet);
        return 1;
    }
    return 0;
}
/**
 * 
 * 1 查找成功
 * 0 查找失败
 */
int hashSetContains(hashSet* hashSet, void* element) {
    return dictFind(hashSet, element) != NULL;
}

size_t hashSetSize(hashSet* hashSet) {
    return dictSize(hashSet);
}


// =========
int hashSetApiAdd(set* set, void* key) {
    return hashSetAdd(set->data, key);
}

int hashSetApiContains(set* set, void* key) {
    return hashSetContains(set->data, key);
}

size_t hashSetApiSize(set* set) {
    return hashSetSize(set->data);
}

int hashSetApiRemove(set* set, void* key) {
    return hashSetRemove(set->data, key);
}

void hashSetApiRelease(set* set) {
    hashSetRelease(set->data);
    zfree(set);
}

typedef struct hashIteratorData {
    hashSetIterator* iter;
    void* currentNode;
} hashIteratorData;

bool hashIteratorApiHasNext(Iterator* iterator) {
    hashIteratorData* data = iterator->data;
    hashSetNode* node = hashSetNext(data->iter);
    if (node == NULL) return false;
    data->currentNode = node;
    return true;
}

void* hashIteratorApiNext(Iterator* iterator) {
    hashIteratorData* data = iterator->data;
    hashSetNode* node = data->currentNode;
    data->currentNode = NULL;
    return node;
}

void hashIteratorApiRelease(Iterator* iterator) {
    hashIteratorData* data = iterator->data;
    hashSetReleaseIterator(data->iter);
    zfree(iterator->data);
    zfree(iterator);
}

IteratorType hashIteratorApi = {
    .hasNext = hashIteratorApiHasNext,
    .next = hashIteratorApiNext,
    .release = hashIteratorApiRelease
};

Iterator* hashSetApiGetIterator(set* set) {
    Iterator* iterator = zmalloc(sizeof(Iterator));
    hashIteratorData* data = zmalloc(sizeof(hashIteratorData));
    data->currentNode = NULL;
    data->iter = hashSetGetIterator(set->data);
    iterator->data = data;
    iterator->type = &hashIteratorApi;
    return iterator;
}

setType hashSetApiType = {
    .add = hashSetApiAdd,
    .contains = hashSetApiContains,
    .size = hashSetApiSize,
    .remove = hashSetApiRemove,
    .release = hashSetApiRelease,
    .getIterator = hashSetApiGetIterator
};
set* setCreateHash(hashSetType* type) {
    set* s = zmalloc(sizeof(set));
    s->data = hashSetCreate(type);
    s->type = &hashSetApiType;
    return s;
}