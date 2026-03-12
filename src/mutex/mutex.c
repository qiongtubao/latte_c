#include "mutex.h"
#include <pthread.h>
#include "zmalloc/zmalloc.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

/**
 * @brief 创建并初始化一个普通（非递归）互斥锁
 * @return latte_mutex_t* 成功返回锁指针，初始化失败释放内存并返回 NULL
 */
latte_mutex_t* latte_mutex_new() {
    latte_mutex_t* mutex = zmalloc(sizeof(latte_mutex_t));
    mutex->attr = NULL; /* 不使用特殊属性（默认非递归） */
    if (0 != latte_mutex_init(mutex)) {
        zfree(mutex);
        return NULL;
    }
    return mutex;
}

/**
 * @brief 为已分配的锁添加递归属性（内部辅助函数）
 * 根据平台选择 PTHREAD_MUTEX_RECURSIVE_NP（Linux）或 PTHREAD_MUTEX_RECURSIVE
 * @param mutex 目标互斥锁指针
 * @return int 成功返回 1，内存分配失败返回 0
 */
int latte_mutex_add_recursive(latte_mutex_t* mutex) {
    mutex->attr = zmalloc(sizeof(pthread_mutexattr_t));
    if (mutex->attr == NULL) {
        return 0;
    }
    pthread_mutexattr_init(mutex->attr);
    /* 根据操作系统设置递归锁类型 */
    #ifdef __linux__
        pthread_mutexattr_settype(mutex->attr, PTHREAD_MUTEX_RECURSIVE_NP);
    #elif defined(__APPLE__) && defined(__MACH__)
        pthread_mutexattr_settype(mutex->attr, PTHREAD_MUTEX_RECURSIVE);
    #else
        pthread_mutexattr_settype(mutex->attr, PTHREAD_MUTEX_RECURSIVE);
    #endif
    return 1;
}

/**
 * @brief 创建并初始化一个支持递归加锁的互斥锁
 * @return latte_mutex_t* 成功返回锁指针，失败返回 NULL 并释放内存
 */
latte_mutex_t* latte_recursive_mutex_new() {
    latte_mutex_t* mutex = zmalloc(sizeof(latte_mutex_t));
    if (mutex == NULL) return NULL;
    /* 先设置递归属性 */
    if (!latte_mutex_add_recursive(mutex)) {
        zfree(mutex);
        return NULL;
    }
    /* 再用带递归属性的 attr 初始化底层锁 */
    if (0 != latte_mutex_init(mutex)) {
        zfree(mutex->attr);
        zfree(mutex);
        return NULL;
    }
    return mutex;
}

/**
 * @brief 使用 mutex->attr 初始化底层 pthread_mutex_t
 * @param mutex 已设置好 attr 的互斥锁指针
 * @return int POSIX 错误码，成功为 0
 */
int latte_mutex_init(latte_mutex_t* mutex) {
    return pthread_mutex_init(&mutex->supper, mutex->attr);
}

/**
 * @brief 阻塞加锁，直到成功获取互斥锁
 * @param mutex 目标互斥锁指针
 */
void latte_mutex_lock(latte_mutex_t* mutex) {
    pthread_mutex_lock(&mutex->supper);
}

/**
 * @brief 释放互斥锁
 * @param mutex 目标互斥锁指针
 */
void latte_mutex_unlock(latte_mutex_t* mutex) {
    pthread_mutex_unlock(&mutex->supper);
}

/**
 * @brief 断言当前线程持有该锁（当前为空实现，仅占位）
 * @param mutex 目标互斥锁指针
 */
void latte_mutex_assert_held(latte_mutex_t* mutex) {
    (void) mutex;
}

/**
 * @brief 销毁底层 pthread_mutex_t（不释放内存）
 * @param mutex 目标互斥锁指针
 * @return int POSIX 错误码，成功为 0
 */
int latte_mutex_destroy(latte_mutex_t* mutex) {
    return pthread_mutex_destroy(&mutex->supper);
}

/**
 * @brief 释放互斥锁：先销毁底层锁，再释放属性和内存
 * @param mutex 要释放的互斥锁指针
 */
void latte_mutex_delete(latte_mutex_t* mutex) {
    assert(latte_mutex_destroy(mutex) == Mutex_Ok);
    if (mutex->attr != NULL) {
        pthread_mutexattr_destroy(mutex->attr);
    }
    zfree(mutex);
}

/**
 * @brief 非阻塞尝试加锁
 * @param mutex 目标互斥锁指针
 * @return int 成功获取锁返回 0，锁已被持有返回 EBUSY
 */
int latte_mutex_try_lock(latte_mutex_t* mutex) {
    return pthread_mutex_trylock(&mutex->supper);
}

/* ==================== 读写锁实现 ==================== */

/**
 * @brief 创建并初始化一个读写锁
 * 内部使用递归互斥锁，支持写锁递归加锁。
 * @return latte_shared_mutex_t* 成功返回锁指针，失败返回 NULL
 */
latte_shared_mutex_t* latte_shared_mutex_new() {
    latte_shared_mutex_t* share_mutex = zmalloc(sizeof(latte_shared_mutex_t));
    if (share_mutex == NULL) return NULL;
    /* 初始化条件变量，用于读写锁等待/唤醒 */
    pthread_cond_init(&share_mutex->shared_lock_cv, NULL);
    share_mutex->shared_lock_count = 0;
    share_mutex->exclusive_lock_count = 0;
    /* 内部互斥锁必须是递归锁，以支持写锁重入 */
    if (!latte_mutex_add_recursive(&share_mutex->supper)) {
        zfree(share_mutex);
        return NULL;
    }
    return share_mutex;
}

/**
 * @brief 释放读写锁，销毁内部互斥锁和条件变量
 * @param share_mutex 要释放的读写锁指针
 */
void latte_shared_mutex_delete(latte_shared_mutex_t* share_mutex) {
    latte_mutex_destroy(&share_mutex->supper);
    pthread_cond_destroy(&share_mutex->shared_lock_cv);
    zfree(share_mutex);
}

/**
 * @brief 加写锁（独占锁），等待所有读锁和写锁都释放后才能获取
 * @param share_mutex 目标读写锁指针
 */
void latte_shared_mutex_lock(latte_shared_mutex_t* share_mutex) {
    latte_mutex_lock(&share_mutex->supper);
    /* 等待读锁计数和写锁计数都归零 */
    while (share_mutex->shared_lock_count > 0 || share_mutex->exclusive_lock_count > 0) {
        pthread_cond_wait(&share_mutex->shared_lock_cv, &share_mutex->supper.supper);
    }
    share_mutex->exclusive_lock_count++;
    latte_mutex_unlock(&share_mutex->supper);
}

/**
 * @brief 释放写锁，若写锁计数归零则广播唤醒所有等待线程
 * @param share_mutex 目标读写锁指针
 */
void latte_shared_mutex_unlock(latte_shared_mutex_t* share_mutex) {
    latte_mutex_lock(&share_mutex->supper);
    if (--share_mutex->exclusive_lock_count == 0) {
        /* 写锁完全释放，唤醒等待的读/写线程 */
        pthread_cond_broadcast(&share_mutex->shared_lock_cv);
    }
    latte_mutex_unlock(&share_mutex->supper);
}

/**
 * @brief 加读锁（共享锁），等待所有写锁释放后才能获取
 * @param share_mutex 目标读写锁指针
 */
void latte_shared_mutex_lock_shared(latte_shared_mutex_t* share_mutex) {
    latte_mutex_lock(&share_mutex->supper);
    /* 等待写锁计数归零 */
    while (share_mutex->exclusive_lock_count > 0) {
        pthread_cond_wait(&share_mutex->shared_lock_cv, &share_mutex->supper.supper);
    }
    share_mutex->shared_lock_count++;
    latte_mutex_unlock(&share_mutex->supper);
}

/**
 * @brief 释放读锁，若读锁计数归零则广播唤醒等待的写线程
 * @param share_mutex 目标读写锁指针
 */
void latte_shared_mutex_unlock_shared(latte_shared_mutex_t* share_mutex) {
    latte_mutex_lock(&share_mutex->supper);
    if (--share_mutex->shared_lock_count < 0) {
        share_mutex->shared_lock_count = 0; /* 防止计数降为负数 */
    }
    if (share_mutex->shared_lock_count == 0) {
        /* 所有读锁已释放，唤醒等待写锁的线程 */
        pthread_cond_broadcast(&share_mutex->shared_lock_cv);
    }
    latte_mutex_unlock(&share_mutex->supper);
}

/**
 * @brief 尝试非阻塞加读锁
 * 若当前有写锁持有则立即返回失败，否则增加读锁计数。
 * @param share_mutex 目标读写锁指针
 * @return int 成功获取读锁返回 1，当前有写锁返回 0
 */
int latte_shared_mutex_try_lock_shared(latte_shared_mutex_t* share_mutex) {
    int result = 0;
    latte_mutex_lock(&share_mutex->supper);
    /* 无写锁时才能加读锁 */
    if (share_mutex->exclusive_lock_count == 0) {
        share_mutex->shared_lock_count++;
        result = 1;
    }
    latte_mutex_unlock(&share_mutex->supper);
    return result;
}
