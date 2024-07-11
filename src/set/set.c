#include "set.h"
#include "zmalloc/zmalloc.h"
#include "dict/dict.h"
#include "dict_plugins/dict_plugins.h"
#include "sds/sds.h"
#include <stdlib.h>
#include <string.h>

/* Set dictionary type. Keys are SDS strings, values are ot used. */
dictType sdsSetDictType = {
    dictSdsHash,               /* hash function */
    dictSdsDup,                      /* key dup */
    NULL,                      /* val dup */
    dictSdsKeyCompare,         /* key compare */
    dictSdsDestructor,         /* key destructor */
    NULL                       /* val destructor */
};


set* setCreate(dictType* dt) {
    set* s = zmalloc(sizeof(set));
    setInit(s, dt);
    return s;
}

void setRelease(set* set) {
    dictRelease(set);
}

void setInit(set* set, dictType* dt) {
    dictInit(set, dt);
}
void setDestroy(set* set) {
    dictDestroy(set);
}

/**
 *  return 
 *  0 表示原来已经有了
 *  1 表示新加
 */
int setAdd(set* set, void* element) {
    dictEntry *de = dictAddRaw(set,element,NULL);
    if (de) {
        dictSetVal(set,de,NULL);
        return 1;
    }
    return 0;
}
/**
 *  1 删除成功
 *  0 删除失败
 */
int setRemove(set* set, void* element) {
    if (dictDelete(set ,element) == DICT_OK) {
        if (htNeedsResize(set)) dictResize(set);
        return 1;
    }
    return 0;
}
/**
 * 
 * 1 查找成功
 * 0 查找失败
 */
int setContains(set* set, void* element) {
    return dictFind(set, element) != NULL;
}

size_t setSize(set* set) {
    return dictSize(set);
}