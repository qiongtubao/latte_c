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
int hashSetApiAdd(set_t* set, void* key) {
    return hashSetAdd(set->data, key);
}

int hashSetApiContains(set_t* set, void* key) {
    return hashSetContains(set->data, key);
}

size_t hashSetApiSize(set_t* set) {
    return hashSetSize(set->data);
}

int hashSetApiRemove(set_t* set, void* key) {
    return hashSetRemove(set->data, key);
}

void hashSetApiRelease(set_t* set) {
    hashSetRelease(set->data);
    zfree(set);
}

typedef struct hashIteratorData {
    hashSetIterator* iter;
    void* currentNode;
} hashIteratorData;

bool hashIteratorApiHasNext(latte_iterator_t* iterator) {
    hashIteratorData* data = iterator->data;
    hashSetNode* node = hashSetNext(data->iter);
    if (node == NULL) return false;
    data->currentNode = node;
    return true;
}

void* hashIteratorApiNext(latte_iterator_t* iterator) {
    hashIteratorData* data = iterator->data;
    hashSetNode* node = data->currentNode;
    data->currentNode = NULL;
    return node;
}

void hashIteratorApiRelease(latte_iterator_t* iterator) {
    hashIteratorData* data = iterator->data;
    hashSetReleaseIterator(data->iter);
    zfree(iterator->data);
    zfree(iterator);
}

latte_iterator_func hashIteratorApi = {
    .hasNext = hashIteratorApiHasNext,
    .next = hashIteratorApiNext,
    .release = hashIteratorApiRelease
};

latte_iterator_t* hashSetApiGetIterator(set_t* set) {
    latte_iterator_t* iterator = zmalloc(sizeof(latte_iterator_t));
    hashIteratorData* data = zmalloc(sizeof(hashIteratorData));
    data->currentNode = NULL;
    data->iter = hashSetGetIterator(set->data);
    iterator->data = data;
    iterator->type = &hashIteratorApi;
    return iterator;
}

set_func_t hashSetApiType = {
    .add = hashSetApiAdd,
    .contains = hashSetApiContains,
    .size = hashSetApiSize,
    .remove = hashSetApiRemove,
    .release = hashSetApiRelease,
    .getIterator = hashSetApiGetIterator
};
set_t* set_newHash(hashSetType* type) {
    set_t* s = zmalloc(sizeof(set_t));
    s->data = hashSetCreate(type);
    s->type = &hashSetApiType;
    return s;
}