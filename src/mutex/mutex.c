#include "mutex.h"
#include <pthread.h>
#include "zmalloc/zmalloc.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

latte_mutex_t* latte_mutex_new() {
    latte_mutex_t* mutex = zmalloc(sizeof(latte_mutex_t));
    mutex->attr = NULL;
    if (0 != latte_mutex_init(mutex)) {
        zfree(mutex);
        return NULL;
    }
    return mutex;
}

latte_mutex_t* latte_recursive_mutex_new() {
    latte_mutex_t* mutex = zmalloc(sizeof(latte_mutex_t));
    mutex->attr = zmalloc(sizeof(pthread_mutexattr_t));
    pthread_mutexattr_init(mutex->attr);
    //暂时没对系统做兼容  其他系统使用的PTHREAD_MUTEX_RECURSIVE
    #ifdef __linux__
        pthread_mutexattr_settype(mutex->attr, PTHREAD_MUTEX_RECURSIVE_NP);
    #elif defined(__APPLE__) && defined(__MACH__)
        pthread_mutexattr_settype(mutex->attr, PTHREAD_MUTEX_RECURSIVE);
    #else
        pthread_mutexattr_settype(mutex->attr, PTHREAD_MUTEX_RECURSIVE);
    #endif
    if (0 != latte_mutex_init(mutex)) {
        zfree(mutex->attr);
        zfree(mutex);
        return NULL;
    }
    return mutex;
}

int latte_mutex_init(latte_mutex_t* mutex) {
    return pthread_mutex_init(mutex, mutex->attr);
}

void latte_mutex_lock(latte_mutex_t* mutex) {
    pthread_mutex_lock(mutex); 
}
void latte_mutex_unlock(latte_mutex_t* mutex) {
    pthread_mutex_unlock(mutex);  
}
void latte_mutex_assert_held(latte_mutex_t* mutex) {
    (void) mutex;
}
int latte_mutex_destroy(latte_mutex_t* mutex) {
    return pthread_mutex_destroy(mutex);
}
void latte_mutex_delete(latte_mutex_t* mutex) {
    assert(latte_mutex_destroy(mutex) == Mutex_Ok);
    if (mutex->attr != NULL) {
        pthread_mutexattr_destroy(mutex->attr);
    }
    zfree(mutex);
}

int latte_mutex_try_lock(latte_mutex_t* mutex) {
    return pthread_mutex_trylock(mutex);
}