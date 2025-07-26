

#ifndef __LATTE_THREAD_CONSUMER_JOB_H
#define __LATTE_THREAD_CONSUMER_JOB_H

#include "thread_job/thread_job.h"
#include "utils/atomic.h"
#include "utils/config.h"
#include "thread_pool/thread_pool.h"


/**
 * 线程消费者管理器
 * 
 *  主线程发送任务
 *  子线程消费任务
 *      参数如何处理
 *      由处理函数释放掉

 * 
 */

typedef struct latte_job_consumer_queue_t {
    latte_job_t* ring_buffer;
    size_t size;
    latteAtomic size_t head __attribute__((aligned(CACHE_LINE_SIZE))); /* Next write index for producer (main-thread) */
    latteAtomic size_t tail __attribute__((aligned(CACHE_LINE_SIZE))); /* Next read index for consumer (bg-thread) */
} latte_job_consumer_queue_t;
latte_job_consumer_queue_t* latte_job_consumer_queue_new(size_t size);
void latte_job_consumer_queue_delete(latte_job_consumer_queue_t* queue);
void latte_job_consumer_queue_push(latte_job_consumer_queue_t* queue, latte_job_t* job);
size_t latte_job_consumer_queue_available(latte_job_consumer_queue_t* queue);
void latte_job_consumer_queue_peek(latte_job_consumer_queue_t* queue, latte_job_t* job);
void latte_job_consumer_queue_remove(latte_job_consumer_queue_t* queue);
bool latte_job_consumer_queue_is_empty(latte_job_consumer_queue_t* queue);
bool latte_job_consumer_queue_is_full(latte_job_consumer_queue_t* queue);

typedef struct latte_thread_consumer_job_manager_t {
    thread_pool_t* pool;
    size_t active_io_threads_num;
    long long push_count;
} latte_thread_consumer_job_manager_t;

latte_thread_consumer_job_manager_t* latte_thread_consumer_job_manager_new(size_t min_thread_num, size_t max_thread_num);
void latte_thread_consumer_job_manager_delete(latte_thread_consumer_job_manager_t* manager);
void latte_thread_consumer_job_manager_push(latte_thread_consumer_job_manager_t* manager, latte_job_t* job);
void latte_thread_consumer_job_manager_adjust(latte_thread_consumer_job_manager_t* manager, size_t target_thread_num);
#endif