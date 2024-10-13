#include "lockSet.h"

lockSet* lockSetCreate(set_t* s) {
    lockSet* lockset = zmalloc(sizeof(lockSet));
    lockSetInit(lockset, s);
    return lockset;
}

int lockSetInit(lockSet* lockset, set_t* s) {
    lockset->set = s;
    return latte_mutex_init(&lockset->mutex);
}

void lockSetDestroy(lockSet* lockset) {
    latte_mutex_lock(&lockset->mutex);
    set_t* s = lockset->set;
    lockset->set = NULL;
    latte_mutex_unlock(&lockset->mutex);
    set_delete(s);
    latte_mutex_destroy(&lockset->mutex);
}
void lockSetRelease(lockSet* lockset) {
    lockSetDestroy(lockset);
    zfree(lockset);
}

bool lockSetAdd(lockSet* set, void* element) {
    latte_mutex_lock(&set->mutex);
    bool result = set_add(set->set, element);
    latte_mutex_unlock(&set->mutex);
    return result;
}

bool lockSetRemove(lockSet* set, void* element) {
    latte_mutex_lock(&set->mutex);
    bool result = set_remove(set->set, element);
    latte_mutex_unlock(&set->mutex);
    return result;
}

bool lockSetContains(lockSet* set, void* element) {
    latte_mutex_lock(&set->mutex);
    bool result = set_contains(set->set, element);
    latte_mutex_unlock(&set->mutex);
    return result;
}

size_t lockSetSize(lockSet* set) {
    latte_mutex_lock(&set->mutex);
    size_t result = set_size(set->set);
    latte_mutex_unlock(&set->mutex);
    return result; 
}