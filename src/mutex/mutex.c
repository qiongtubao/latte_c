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

int latte_mutex_add_recursive(latte_mutex_t* mutex) {
    mutex->attr = zmalloc(sizeof(pthread_mutexattr_t));
    if (mutex->attr == NULL) {
        return 0;
    }
    pthread_mutexattr_init(mutex->attr);
    //暂时没对系统做兼容  其他系统使用的PTHREAD_MUTEX_RECURSIVE
    #ifdef __linux__
        pthread_mutexattr_settype(mutex->attr, PTHREAD_MUTEX_RECURSIVE_NP);
    #elif defined(__APPLE__) && defined(__MACH__)
        pthread_mutexattr_settype(mutex->attr, PTHREAD_MUTEX_RECURSIVE);
    #else
        pthread_mutexattr_settype(mutex->attr, PTHREAD_MUTEX_RECURSIVE);
    #endif
    return 1;
}
latte_mutex_t* latte_recursive_mutex_new() {
    latte_mutex_t* mutex = zmalloc(sizeof(latte_mutex_t));
    if (mutex == NULL) return NULL;
    if (!latte_mutex_add_recursive(mutex)) {
        zfree(mutex);
        return NULL;
    }
    if (0 != latte_mutex_init(mutex)) {
        zfree(mutex->attr);
        zfree(mutex);
        return NULL;
    }
    return mutex;
}

int latte_mutex_init(latte_mutex_t* mutex) {
    return pthread_mutex_init(&mutex->supper, mutex->attr);
}

void latte_mutex_lock(latte_mutex_t* mutex) {
    pthread_mutex_lock(&mutex->supper); 
}
void latte_mutex_unlock(latte_mutex_t* mutex) {
    pthread_mutex_unlock(&mutex->supper);  
}
void latte_mutex_assert_held(latte_mutex_t* mutex) {
    (void) mutex;
}
int latte_mutex_destroy(latte_mutex_t* mutex) {
    return pthread_mutex_destroy(&mutex->supper);
}
void latte_mutex_delete(latte_mutex_t* mutex) {
    assert(latte_mutex_destroy(mutex) == Mutex_Ok);
    if (mutex->attr != NULL) {
        pthread_mutexattr_destroy(mutex->attr);
    }
    zfree(mutex);
}

int latte_mutex_try_lock(latte_mutex_t* mutex) {
    return pthread_mutex_trylock(&mutex->supper);
}


//shared_mutex

latte_shared_mutex_t* latte_shared_mutex_new() {
    latte_shared_mutex_t* share_mutex =  zmalloc(sizeof(latte_shared_mutex_t));
    if (share_mutex == NULL) return NULL;
    pthread_cond_init(&share_mutex->shared_lock_cv, NULL);
    share_mutex->shared_lock_count = 0;
    share_mutex->exclusive_lock_count = 0;
    if (!latte_mutex_add_recursive(&share_mutex->supper)) {
        zfree(share_mutex);
        return NULL;
    }
    return share_mutex;
}
void latte_shared_mutex_delete(latte_shared_mutex_t* share_mutex) {
    latte_mutex_destroy(&share_mutex->supper);
    pthread_cond_destroy(&share_mutex->shared_lock_cv);
    zfree(share_mutex);
}

void latte_shared_mutex_lock(latte_shared_mutex_t* share_mutex) {
    latte_mutex_lock(&share_mutex->supper);
    while (share_mutex->shared_lock_count > 0 || share_mutex->exclusive_lock_count > 0) {
        pthread_cond_wait(&share_mutex->shared_lock_cv, &share_mutex->supper.supper);
    }
    share_mutex->exclusive_lock_count++;
    latte_mutex_unlock(&share_mutex->supper);
}

void latte_shared_mutex_unlock(latte_shared_mutex_t* share_mutex) {
    latte_mutex_lock(&share_mutex->supper);
    if (--share_mutex->exclusive_lock_count == 0) {
        pthread_cond_broadcast(&share_mutex->shared_lock_cv);
    }
    latte_mutex_unlock(&share_mutex->supper);
}

void latte_shared_mutex_lock_shared(latte_shared_mutex_t* share_mutex) {
    latte_mutex_lock(&share_mutex->supper);
    while (share_mutex->exclusive_lock_count > 0) {
        pthread_cond_wait(&share_mutex->shared_lock_cv, &share_mutex->supper.supper);
    }
    share_mutex->shared_lock_count++;
    latte_mutex_unlock(&share_mutex->supper);
}

void latte_shared_mutex_unlock_shared(latte_shared_mutex_t* share_mutex) {
    latte_mutex_lock(&share_mutex->supper);
    if (--share_mutex->shared_lock_count < 0) {
        share_mutex->shared_lock_count = 0;  // Ensure it does not go below zero
    }
    if (share_mutex->shared_lock_count == 0) {
        pthread_cond_broadcast(&share_mutex->shared_lock_cv);
    }
    latte_mutex_unlock(&share_mutex->supper);
}

int latte_shared_mutex_try_lock_shared(latte_shared_mutex_t* share_mutex) {
    int result = 0;
    latte_mutex_lock(&share_mutex->supper);
    if (share_mutex->exclusive_lock_count == 0) {
        share_mutex->shared_lock_count++;
        result = 1;
    }
    latte_mutex_unlock(&share_mutex->supper);
    return result;
}
