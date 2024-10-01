#include "lockSet.h"

lockSet* lockSetCreate(set* s) {
    lockSet* lockset = zmalloc(sizeof(lockSet));
    lockSetInit(lockset, s);
    return lockset;
}

int lockSetInit(lockSet* lockset, set* s) {
    lockset->set = s;
    return mutexInit(&lockset->mutex);
}

void lockSetDestroy(lockSet* lockset) {
    mutexLock(&lockset->mutex);
    set* s = lockset->set;
    lockset->set = NULL;
    mutexUnlock(&lockset->mutex);
    setRelease(s);
    mutexDestroy(&lockset->mutex);
}
void lockSetRelease(lockSet* lockset) {
    lockSetDestroy(lockset);
    zfree(lockset);
}

bool lockSetAdd(lockSet* set, void* element) {
    mutexLock(&set->mutex);
    bool result = setAdd(set->set, element);
    mutexUnlock(&set->mutex);
    return result;
}

bool lockSetRemove(lockSet* set, void* element) {
    mutexLock(&set->mutex);
    bool result = setRemove(set->set, element);
    mutexUnlock(&set->mutex);
    return result;
}

bool lockSetContains(lockSet* set, void* element) {
    mutexLock(&set->mutex);
    bool result = setContains(set->set, element);
    mutexUnlock(&set->mutex);
    return result;
}

size_t lockSetSize(lockSet* set) {
    mutexLock(&set->mutex);
    size_t result = setSize(set->set);
    mutexUnlock(&set->mutex);
    return result; 
}