

#ifndef __LATTE_THREAD_POOL_H
#define __LATTE_THREAD_POOL_H

#include "thread_config.h"
#include <stdint.h>
#include "vector/vector.h"
#include <pthread.h>
#include "list/list.h"
#include "mutex/mutex.h"
#include "error/error.h"
#include "utils/atomic.h"
#include "ae/ae.h"

// typedef void *(latte_thread_func) (latte_thread_t *arg); 

typedef struct {
    size_t thread_count;   //当前线程数
    size_t core_pool_size;    //最小线程数
    size_t max_pool_size;    //最大线程数
    size_t create_thread_min_work_size; //创建线程所需最少任务数
    size_t remove_thread_min_kepp_alive_time;//删除线程所需的最少等待数
    vector_t* threads;
    // latte_thread_func *thread_main;
    latte_mutex_t* callback_lock;
    list_t* callback_queue;
    int recv_fd;
    int callback_fd;
} latte_thread_pool_t;

typedef latte_error_t* (latte_thread_exec)(void* arg, void** result);
typedef void (latte_thread_callback)(latte_error_t* error, void* result);
typedef void (latte_thread_task_free)(void* task);
typedef struct {
    latte_thread_exec* exec;    //任务函数
    latte_thread_callback* callback;//回调函数
    latte_thread_task_free* free; //释放函数
    void *arg;
    latte_error_t* error;
    void *result;
} latte_thread_task_t;
latte_thread_task_t* latte_thread_task_new(latte_thread_exec* exec , void* arg, latte_thread_callback* callback, latte_thread_task_free* free);
void latte_thread_task_delete(latte_thread_task_t *callback);



typedef struct latte_thread_t {
    latteAtomic long current_tasks;
    list_t* task_queue;
    pthread_t thread_id;
    size_t idle_time;
    latte_mutex_t* lock;
    pthread_cond_t cond;
    int id;
    latte_thread_pool_t* pool;
} latte_thread_t;

latte_thread_t* latte_thread_new(latte_thread_pool_t* pool);
void latte_thread_delete(latte_thread_t* t);



void* latte_thread_create(void* arg);
latte_thread_pool_t* thread_pool_new(size_t core_pool_size, 
    size_t max_pool_size, 
    size_t create_thread_min_work_size,
    size_t remove_thread_min_kepp_alive_time,
    aeEventLoop* ae);
latte_error_t* thread_pool_submit(latte_thread_pool_t* pool, latte_thread_task_t* task);

#endif