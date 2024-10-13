#include "hashSet.h"
#include "zmalloc/zmalloc.h"

/* hashSet dictionary type. Keys are SDS strings, values are ot used. */
hashSetType sdsHashSetDictType = {
    dict_sds_hash,               /* hash function */
    dict_sds_dup,                      /* key dup */
    NULL,                      /* val dup */
    dict_sds_key_compare,         /* key compare */
    dict_sds_destructor,         /* key destructor */
    NULL                       /* val destructor */
};


hashSet* hashSetCreate(hashSetType* dt) {
    hashSet* s = zmalloc(sizeof(hashSet));
    hashSetInit(s, dt);
    return s;
}

void hashSetRelease(hashSet* hashSet) {
    dict_delete(hashSet);
}

void hashSetInit(hashSet* s, dict_func_t* dt) {
    _dict_init(s, dt);
}
void hashSetDestroy(hashSet* hashSet) {
    dict_destroy(hashSet);
}

/**
 *  return 
 *  0 表示原来已经有了
 *  1 表示新加
 */
int hashSetAdd(hashSet* hashSet, void* element) {
    dict_entry_t*de = dict_add_raw(hashSet,element,NULL);
    if (de) {
        dict_set_val(hashSet,de,NULL);
        return 1;
    }
    return 0;
}
/**
 *  1 删除成功
 *  0 删除失败
 */
int hashSetRemove(hashSet* hashSet, void* element) {
    if (dict_delete_key(hashSet ,element) == DICT_OK) {
        if (ht_needs_resize(hashSet)) dict_resize(hashSet);
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
    return dict_find(hashSet, element) != NULL;
}

size_t hashSetSize(hashSet* hashSet) {
    return dict_size(hashSet);
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