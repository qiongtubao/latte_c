#include "mutex.h"
#include <pthread.h>
#include "zmalloc/zmalloc.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
latte_mutex* mutexCreate() {
    latte_mutex* mutex = zmalloc(sizeof(latte_mutex));
    mutex->attr = NULL;
    if (0 != mutexInit(mutex)) {
        zfree(mutex);
        return NULL;
    }
    return mutex;
}

latte_mutex* mutextRecursiveCreate() {
    latte_mutex* mutex = zmalloc(sizeof(latte_mutex));
    mutex->attr = zmalloc(sizeof(pthread_mutexattr_t));
    pthread_mutexattr_init(mutex->attr);
    //暂时没对系统做兼容  其他系统使用的PTHREAD_MUTEX_RECURSIVE
    #if defined(__APPLE__) || defined(__FreeBSD__)
        pthread_mutexattr_settype_np(mutex->attr, PTHREAD_MUTEX_RECURSIVE_NP);
    #else
        pthread_mutexattr_settype(mutex->attr, PTHREAD_MUTEX_RECURSIVE);
    #endif
    if (0 != mutexInit(mutex)) {
        zfree(mutex->attr);
        zfree(mutex);
        return NULL;
    }
    return mutex;
}

int mutexInit(latte_mutex* mutex) {
    return pthread_mutex_init(mutex, mutex->attr);
}

void mutexLock(latte_mutex* mutex) {
    pthread_mutex_lock(mutex); 
}
void mutexUnlock(latte_mutex* mutex) {
    pthread_mutex_unlock(mutex);  
}
void mutexAssertHeld(latte_mutex* mutex) {
    (void) mutex;
}
int mutexDestroy(latte_mutex* mutex) {
    return pthread_mutex_destroy(mutex);
}
void mutexRelease(latte_mutex* mutex) {
    assert(mutexDestroy(mutex) == Mutex_Ok);
    if (mutex->attr != NULL) {
        pthread_mutexattr_destroy(mutex->attr);
    }
    zfree(mutex);
}

int mutexTrylock(latte_mutex* mutex) {
    return pthread_mutex_trylock(mutex);
}