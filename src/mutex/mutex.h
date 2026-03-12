
#ifndef LATTE_MUTEX_H
#define LATTE_MUTEX_H
#include <pthread.h>

/**
 * @brief latte 互斥锁结构体，封装 pthread_mutex_t 及其属性
 * attr 为 NULL 时使用默认（非递归）互斥锁；
 * 设置了 PTHREAD_MUTEX_RECURSIVE 属性时支持同一线程递归加锁。
 */
typedef struct latte_mutex_t {
  pthread_mutex_t supper;       /**< 底层 POSIX 互斥锁 */
  pthread_mutexattr_t* attr;    /**< 互斥锁属性指针，NULL 表示使用默认属性 */
} latte_mutex_t;

/**
 * @brief 创建并初始化一个普通（非递归）互斥锁
 * @return latte_mutex_t* 成功返回新锁指针，初始化失败返回 NULL
 */
latte_mutex_t* latte_mutex_new();

/**
 * @brief 创建并初始化一个支持同一线程递归加锁的互斥锁
 * @return latte_mutex_t* 成功返回新锁指针，失败返回 NULL
 */
latte_mutex_t* latte_recursive_mutex_new();

/**
 * @brief 释放互斥锁及其内存（会先调用 destroy）
 * @param mutex 要释放的互斥锁指针
 */
void latte_mutex_delete(latte_mutex_t*);

/**
 * @brief 初始化互斥锁（使用已分配的 latte_mutex_t 结构体）
 * @param mutex 已分配内存的互斥锁指针
 * @return int 成功返回 0，失败返回 POSIX 错误码
 */
int latte_mutex_init(latte_mutex_t* mutex);

/**
 * @brief 销毁互斥锁（释放内核资源，但不释放内存）
 * 可能的返回值：
 *   0       - 成功
 *   EINVAL  - 锁未正确初始化
 *   EBUSY   - 锁当前被持有，无法销毁
 * @param mutex 目标互斥锁指针
 * @return int POSIX 错误码，成功为 0
 */
int latte_mutex_destroy(latte_mutex_t* mutex);

/**
 * @brief 尝试非阻塞加锁
 * 可能的返回值：
 *   0       - 成功获取锁
 *   EBUSY   - 锁已被其他线程持有
 *   EDEADLK - 检测到死锁（非递归锁重复加锁）
 *   EINVAL  - 无效参数
 *   EPERM   - 权限不足
 * @param mutex 目标互斥锁指针
 * @return int 成功返回 0，失败返回对应错误码
 */
int latte_mutex_try_lock(latte_mutex_t* mutex);

/**
 * @brief 阻塞加锁，直到成功获取锁
 * @param mutex 目标互斥锁指针
 */
void latte_mutex_lock(latte_mutex_t* mutex);

/**
 * @brief 释放互斥锁
 * @param mutex 目标互斥锁指针
 */
void latte_mutex_unlock(latte_mutex_t* mutex);

/**
 * @brief 断言当前线程持有该锁（当前实现为空操作）
 * @param mutex 目标互斥锁指针
 */
void latte_mutex_assert_held(latte_mutex_t* mutex);

/**
 * @brief 支持写锁递归加锁的读写锁
 *
 * 特性：
 * - 读锁（shared lock）可被多个线程同时持有，且可递归加锁
 * - 写锁（exclusive lock）同一线程可递归加锁（因内部使用递归互斥锁）
 * - 持有读锁的线程不能再加写锁
 * - 写锁会等待所有读锁和其他写锁释放后才能获取
 * - 仅在 CONCURRENCY 编译模式下真正生效
 */
typedef struct latte_shared_mutex_t {
    latte_mutex_t supper;            /**< 底层递归互斥锁，保护内部状态 */
    pthread_cond_t shared_lock_cv;   /**< 条件变量，用于读/写锁等待通知 */
    int exclusive_lock_count;        /**< 当前写锁重入计数（同一线程可多次加写锁） */
    int shared_lock_count;           /**< 当前持有读锁的线程计数 */
} latte_shared_mutex_t;

/**
 * @brief 创建并初始化一个读写锁
 * @return latte_shared_mutex_t* 成功返回新锁指针，失败返回 NULL
 */
latte_shared_mutex_t* latte_shared_mutex_new();

/**
 * @brief 释放读写锁及其内存
 * @param share_mutex 要释放的读写锁指针
 */
void latte_shared_mutex_delete(latte_shared_mutex_t* share_mutex);

/**
 * @brief 加写锁（独占锁），会等待所有读锁和写锁释放
 * @param share_mutex 目标读写锁指针
 */
void latte_shared_mutex_lock(latte_shared_mutex_t* share_mutex);

/**
 * @brief 释放写锁，若计数归零则广播唤醒等待线程
 * @param share_mutex 目标读写锁指针
 */
void latte_shared_mutex_unlock(latte_shared_mutex_t* share_mutex);

/**
 * @brief 加读锁（共享锁），会等待所有写锁释放后才能获取
 * @param share_mutex 目标读写锁指针
 */
void latte_shared_mutex_lock_shared(latte_shared_mutex_t* share_mutex);

/**
 * @brief 释放读锁，若读计数归零则广播唤醒等待线程
 * @param share_mutex 目标读写锁指针
 */
void latte_shared_mutex_unlock_shared(latte_shared_mutex_t* share_mutex);

/**
 * @brief 尝试非阻塞加读锁
 * @param share_mutex 目标读写锁指针
 * @return int 成功获取读锁返回 1，当前有写锁持有则返回 0
 */
int latte_shared_mutex_try_lock_shared(latte_shared_mutex_t* share_mutex);

/** @brief mutex 操作成功的返回值常量 */
#define Mutex_Ok 0

#endif