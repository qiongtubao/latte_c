#include "mutex.h"
#include <pthread.h>
#include "zmalloc/zmalloc.h"
#include <assert.h>
latte_mutex* mutexCreate() {
    latte_mutex* mutex = zmalloc(sizeof(latte_mutex));
    if (0 != mutexInit(mutex)) {
        zfree(mutex);
        return NULL;
    }
    return mutex;
}

int mutexInit(latte_mutex* mutex) {
    return pthread_mutex_init(mutex, NULL);
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
    zfree(mutex);
}

int mutexTrylock(latte_mutex* mutex) {
    return pthread_mutex_trylock(mutex);
}