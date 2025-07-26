

#ifndef __LATTE_THREAD_POOL_H
#define __LATTE_THREAD_POOL_H

#include <pthread.h>
#include "vector/vector.h"
#include "utils/atomic.h"
#include "time/localtime.h"
#include "sds/sds.h"

static int thread_id = 0;
bool is_main_thread();
int  get_thread_id();


#define LATTE_THREAD_RUNNING 0
#define LATTE_THREAD_STOP 1

typedef struct latte_thread_t {
    int tid;
    pthread_t thread;
    sds_t name;
    void* (*run)(void* ctx);
    int status; 
    void* ctx;
    pthread_t thread_id;
} latte_thread_t;

typedef struct thread_pool_t {
    vector_t* threads; //栈结构  扩容添加到最后一个线程，缩容也是先从最后一个线程开始删除
    latte_thread_t* (*create_thread_func)();
    void (*delete_thread_func)(latte_thread_t* thread);
    size_t max_thread_num;
    size_t min_thread_num;
} thread_pool_t;
thread_pool_t* latte_thread_pool_new(size_t min_thread_num, 
    size_t max_thread_num, 
    latte_thread_t* (*create_thread_func)(int tid), 
    void (*delete_thread_func)(latte_thread_t* thread)
);
bool latte_thread_pool_add_thread(thread_pool_t* pool);
bool latte_thread_pool_delete_thread(thread_pool_t* pool);
void latte_thread_pool_delete(thread_pool_t* pool);


#endif