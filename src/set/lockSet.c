#include "lockSet.h"

lockSet* lockSetCreate(dictType* dt) {
    lockSet* lockset = zmalloc(sizeof(lockSet));
    lockSetInit(lockset, dt);
    return lockset;
}

int lockSetInit(lockSet* lockset, dictType* dt) {
    setInit(&lockset->set, dt);
    return mutexInit(&lockset->mutex);
}

void lockSetDestroy(lockSet* lockset) {
    setDestroy(&lockset->set);
    mutexDestroy(&lockset->mutex);
}
void lockSetRelease(lockSet* lockset) {
    lockSetDestroy(lockset);
    zfree(lockset);
}

bool lockSetAdd(lockSet* set, void* element) {
    mutexLock(&set->mutex);
    bool result = setAdd(&set->set, element);
    mutexUnlock(&set->mutex);
    return result;
}

bool lockSetRemove(lockSet* set, void* element) {
    mutexLock(&set->mutex);
    bool result = setRemove(&set->set, element);
    mutexUnlock(&set->mutex);
    return result;
}

bool lockSetContains(lockSet* set, void* element) {
    mutexLock(&set->mutex);
    bool result = setContains(&set->set, element);
    mutexUnlock(&set->mutex);
    return result;
}

size_t lockSetSize(lockSet* set) {
    mutexLock(&set->mutex);
    size_t result = setSize(&set->set);
    mutexUnlock(&set->mutex);
    return result; 
}