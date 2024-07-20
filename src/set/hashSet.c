#include "hashSet.h"


/* hashSet dictionary type. Keys are SDS strings, values are ot used. */
dictType sdshashSetDictType = {
    dictSdsHash,               /* hash function */
    dictSdsDup,                      /* key dup */
    NULL,                      /* val dup */
    dictSdsKeyCompare,         /* key compare */
    dictSdsDestructor,         /* key destructor */
    NULL                       /* val destructor */
};


hashSet* hashSetCreate(dictType* dt) {
    hashSet* s = zmalloc(sizeof(hashSet));
    hashSetInit(s, dt);
    return s;
}

void hashSetRelease(hashSet* hashSet) {
    dictRelease(hashSet);
}

void hashSetInit(hashSet* hashSet, dictType* dt) {
    dictInit(hashSet, dt);
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
        dicthashSetVal(hashSet,de,NULL);
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