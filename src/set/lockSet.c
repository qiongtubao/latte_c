#include "lockSet.h"

// lockSet* lockSetCreate(set* dt) {
//     lockSet* lockset = zmalloc(sizeof(lockSet));
//     lockSetInit(lockset, dt);
//     return lockset;
// }

// int lockSetInit(lockSet* lockset, set* dt) {
//     setInit(&lockset->set, dt);
//     return mutexInit(&lockset->mutex);
// }

// void lockSetDestroy(lockSet* lockset) {
//     lockset->set->type->release(lockset->set);
//     mutexDestroy(&lockset->mutex);
// }
// void lockSetRelease(lockSet* lockset) {
//     lockSetDestroy(lockset);
//     zfree(lockset);
// }

// bool lockSetAdd(lockSet* set, void* element) {
//     mutexLock(&set->mutex);
//     bool result = set->set->type->add(&set->set, element);
//     mutexUnlock(&set->mutex);
//     return result;
// }

// bool lockSetRemove(lockSet* set, void* element) {
//     mutexLock(&set->mutex);
//     bool result = set->set->type->remove(&set->set, element);
//     mutexUnlock(&set->mutex);
//     return result;
// }

// bool lockSetContains(lockSet* set, void* element) {
//     mutexLock(&set->mutex);
//     bool result = set->set->type->contains(&set->set, element);
//     mutexUnlock(&set->mutex);
//     return result;
// }

// size_t lockSetSize(lockSet* set) {
//     mutexLock(&set->mutex);
//     size_t result = set->set->type->size(&set->set);
//     mutexUnlock(&set->mutex);
//     return result; 
// }