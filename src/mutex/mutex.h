
#ifndef LATTE_MUTEX_H
#define LATTE_MUTEX_H
#include <pthread.h>
// typedef pthread_mutex_t latte_mutex_t;
typedef struct latte_mutex_t {
  pthread_mutex_t supper;
  pthread_mutexattr_t* attr;
} latte_mutex_t;
//new and init
latte_mutex_t* latte_mutex_new();
latte_mutex_t* latte_recursive_mutex_new();
void latte_mutex_delete(latte_mutex_t*);

/**
 * 
 * 
 * 
 */  
int latte_mutex_init(latte_mutex_t* mutex);
/**
  OK (0)
  EINVAL 如果传给pthread_mutex_destroy的锁指针为NULL，或者锁对象没有被正确初始化（例如，不是通过pthread_mutex_init初始化的），某些实现可能会设置errno为EINVAL。但实际上，这种行为是未定义的，因为标准并没有规定在这些条件下必须设置errno。
  EBUSY
*/
int latte_mutex_destroy(latte_mutex_t* mutex);
/**
 * OK (0)
 * EBUSY（忙碌）：如果互斥锁已经被其他线程锁定，函数会立即返回EBUSY，表示锁无法获取。
 * EDEADLK（死锁避免）：在某些实现中，如果检测到请求锁的线程已经持有了该锁（即尝试递归锁定自己持有的锁），可能会返回EDEADLK来避免死锁。不过，是否检测死锁行为依赖于互斥锁的类型和实现。有些类型的锁（如PTHREAD_MUTEX_RECURSIVE类型）是支持递归锁定的，此时不会返回EDEADLK。
 * EINVAL（无效参数）：如果提供的互斥锁指针为NULL，或者它不是一个有效的、由pthread_mutex_init初始化的互斥锁，函数可能返回EINVAL。
 * EPERM（权限不足）：尽管不常见，但某些情况下，如果调用线程没有足够的权限来操作锁，可能会返回EPERM。
 */
int latte_mutex_try_lock(latte_mutex_t* mutex);
void latte_mutex_lock(latte_mutex_t* mutex);
void latte_mutex_unlock(latte_mutex_t* mutex);
void latte_mutex_assert_held(latte_mutex_t* mutex);

#define Mutex_Ok 0


#endif