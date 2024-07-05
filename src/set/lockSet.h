


#ifndef __LATTE_LOCK_SET_H
#define __LATTE_LOCK_SET_H

#include "utils/error.h"
#include "mutex/mutex.h"
#include "set.h"

typedef struct lockSet {
    set set;
    latte_mutex mutex;
} lockSet;

lockSet* lockSetCreate(dictType* dt);
int lockSetInit(lockSet* lockset, dictType* type);
void lockSetDestroy(lockSet* lockset);
void lockSetRelease(lockSet* lockset);

bool lockSetAdd(lockSet* set, void* element);
bool lockSetRemove(lockSet* set, void* element);
bool lockSetContains(lockSet* set, void* element);
size_t lockSetSize(lockSet* set);
#endif