#include "thread_consumer_job.h"
#include "zmalloc/zmalloc.h"
#include "utils/utils.h"
#include "log/log.h"
#include "thread_pool/thread_pool.h"


latte_job_consumer_queue_t* latte_job_consumer_queue_new(size_t size) {
    latte_job_consumer_queue_t* queue = (latte_job_consumer_queue_t*)zmalloc(sizeof(latte_job_consumer_queue_t));
    queue->ring_buffer = (latte_job_t**)zcalloc(sizeof(latte_job_t) * size);
    queue->size = size;
    for (size_t i = 0; i < size; i++) {
        queue->ring_buffer[i].run = NULL;
        queue->ring_buffer[i].argc = 0;
        queue->ring_buffer[i].args = NULL;
    }
    queue->head = 0;
    queue->tail = 0;
    return queue;
}

void latte_job_consumer_queue_delete(latte_job_consumer_queue_t* queue) {
    latte_assert_with_info(queue->tail == queue->head, "queue is not empty");
    latte_assert_with_info(is_main_thread(), "queue is not main thread");
    zfree(queue->ring_buffer);
    zfree(queue);
}

bool latte_job_consumer_queue_is_empty(latte_job_consumer_queue_t* queue) {
    size_t current_head = atomic_load_explicit(&queue->head, memory_order_relaxed);
    size_t current_tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    return current_head == current_tail;
}

bool latte_job_consumer_queue_is_full(latte_job_consumer_queue_t* queue) {
    size_t current_head = atomic_load_explicit(&queue->head, memory_order_relaxed);
    size_t current_tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    size_t next_head = (current_head + 1) % queue->size;
    return next_head == current_tail;
}

void latte_job_consumer_queue_push(latte_job_consumer_queue_t* queue, latte_job_t* job) {
    latte_assert_with_info(!latte_job_consumer_queue_is_full(queue), "queue is full");
    latte_assert_with_info(is_main_thread(), "queue is not main thread");
    size_t current_head = atomic_load_explicit(&queue->head, memory_order_relaxed);
    size_t next_head = (current_head + 1)% queue ->size;
    queue->ring_buffer[current_head].run = job->run;
    queue->ring_buffer[current_head].argc = job->argc;
    queue->ring_buffer[current_head].args = job->args;
    atomic_store_explicit(&queue->head, next_head, memory_order_relaxed);
}




size_t latte_job_consumer_queue_available(latte_job_consumer_queue_t* queue) {
    latte_assert_with_info(!is_main_thread(), "queue is main thread");
    size_t current_head = atomic_load_explicit(&queue->head, memory_order_acquire);
    size_t current_tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    if (current_head>=current_tail) {
        return current_head - current_tail;
    } else {
        return queue->size - (current_tail - current_head);
    }
}

void latte_job_consumer_queue_remove(latte_job_consumer_queue_t* queue) {
    latte_assert_with_info(!is_main_thread(), "queue is main thread");
    size_t current_tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    latte_assert_with_info(queue->ring_buffer[current_tail].run != NULL, "queue is empty");
    queue->ring_buffer[current_tail].run = NULL;
    queue->ring_buffer[current_tail].argc = 0;
    queue->ring_buffer[current_tail].args = NULL;
    atomic_store_explicit(&queue->tail, (current_tail + 1)% queue->size, memory_order_relaxed);
}

void latte_job_consumer_queue_peek(latte_job_consumer_queue_t* queue, latte_job_t* job) {
    latte_assert_with_info(!is_main_thread(), "queue is main thread");
    size_t current_tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    latte_assert_with_info(queue->ring_buffer[current_tail].run != NULL, "queue is empty");
    *job = queue->ring_buffer[current_tail];
}

void* thread_consumer_main(void* arg) {
    latte_thread_t* thread = (latte_thread_t*)arg;
    latte_job_consumer_queue_t* queue = (latte_job_consumer_queue_t*)thread->ctx;
    // char thdname[32];
    // latte_set_cpu_affinity(thread->cpu_list);
    latte_make_thread_killable();
    //init_shared_query_buffer();  // 初始化共享查询缓冲区
    set_thread_id(thread->tid);
    size_t jobs_to_process = 0;
    time_point_t start_time = ustime();
    long long qps = 0;
    while (1) {
        for (size_t i = 0; i < 1000000; i++) {
            jobs_to_process = latte_job_consumer_queue_available(queue);
            if (jobs_to_process > 0) break;
        }

        if (jobs_to_process == 0) {
            if (thread->status == LATTE_THREAD_STOP) {
                return NULL;
            }
            pthread_mutex_lock(&thread->mutex);
            pthread_mutex_unlock(&thread->mutex);
            continue;
        }
        for (size_t i = 0; i < jobs_to_process; i++) {
            latte_job_t job;
            latte_job_consumer_queue_peek(queue, &job);
            latte_job_run(&job);
            latte_job_consumer_queue_remove(queue);
            if (ustime() - start_time > 1000000) {
                LATTE_LIB_LOG(LL_INFO, "qps: %lld", qps);
                qps = 0;
                start_time = ustime();
            } else {
                qps++;
            }
        }

        atomic_thread_fence(memory_order_release); //内存屏障
    }
    //free_shared_query_buffer();  // 释放共享查询缓冲区
    return NULL;
}

latte_thread_t* create_thread_consumer(int tid) {
    latte_thread_t* thread = (latte_thread_t*)zmalloc(sizeof(latte_thread_t));
    thread->tid = tid;
    thread->name = sds_cat_fmt(sds_empty(), "thread_consumer_%d", tid);
    pthread_mutex_init(&thread->mutex, NULL);
    thread->ctx = latte_job_consumer_queue_new(40000);
    pthread_mutex_lock(&thread->mutex);  // 控制线程资源用
    if (pthread_create(&thread->thread_id, NULL, thread_consumer_main, thread)) {
        latte_job_consumer_queue_delete(thread->ctx);
        zfree(thread);
        LATTE_LIB_LOG(LL_ERROR, "create thread consumer failed");
        return NULL;
    }
    LATTE_LIB_LOG(LL_INFO, "create thread consumer success");
    return thread;
}

void delete_thread_consumer(latte_thread_t* thread) {
    thread->status = LATTE_THREAD_STOP;
    pthread_mutex_lock(&thread->mutex);
    int rest = pthread_join(thread->thread_id, NULL);
    latte_assert_with_info(rest == 0, "pthread_join fail");
    latte_job_consumer_queue_delete(thread->ctx);
    pthread_mutex_unlock(&thread->mutex);
    if (thread->name) {
        sds_delete(thread->name);
        thread->name = NULL;
    }
    zfree(thread);
}


latte_thread_consumer_job_manager_t* latte_thread_consumer_job_manager_new(size_t min_thread_num, size_t max_thread_num) {
    latte_thread_consumer_job_manager_t* manager = (latte_thread_consumer_job_manager_t*)zmalloc(sizeof(latte_thread_consumer_job_manager_t));
    manager->pool = latte_thread_pool_new(min_thread_num, max_thread_num, create_thread_consumer, delete_thread_consumer);
    manager->active_io_threads_num = 0;
    manager->push_count = 0;
    return manager;
}

void latte_thread_consumer_job_manager_delete(latte_thread_consumer_job_manager_t* manager) {
    latte_thread_pool_delete(manager->pool);
    zfree(manager);
}

latte_thread_t* select_thread_from_pool(latte_thread_consumer_job_manager_t* manager) {
    latte_thread_t* thread = vector_get(manager->pool->threads, manager->push_count % vector_size(manager->pool->threads));
    return thread;
}

void latte_thread_consumer_job_manager_push(latte_thread_consumer_job_manager_t* manager, latte_job_t* job) {
    latte_thread_t* thread = select_thread_from_pool(manager);
    latte_job_consumer_queue_t* queue = (latte_job_consumer_queue_t*)thread->ctx;
    latte_job_consumer_queue_push(queue, job);
    manager->push_count++;
    
}

void latte_thread_consumer_job_manager_adjust(latte_thread_consumer_job_manager_t* manager, size_t target_thread_num) {
    if (target_thread_num == manager->active_io_threads_num) return;
    if (target_thread_num < manager->active_io_threads_num) {
        int threads_to_deactivate_num = manager->active_io_threads_num - target_thread_num;
        for (int i = 0; i < threads_to_deactivate_num; i++) {
            int tid = manager->active_io_threads_num - 1;
            latte_thread_t* thread = vector_get(manager->pool->threads, tid);
            latte_job_consumer_queue_t* jq = (latte_job_consumer_queue_t*)(thread->ctx);
            /* We can't lock the thread if it may have pending jobs */
            if (!latte_job_consumer_queue_is_empty(jq)) return;
            pthread_mutex_lock(&thread->mutex);
            manager->active_io_threads_num--;
        }

    } else {
        int threads_to_activate_num = target_thread_num - manager->active_io_threads_num;
        for (int i = 0; i < threads_to_activate_num; i++) {
            int tid = manager->active_io_threads_num;
            latte_thread_t* thread = vector_get(manager->pool->threads, tid);
            LATTE_LIB_LOG(LL_INFO, "%d unlock %d", thread->tid, tid);
            pthread_mutex_unlock(&thread->mutex);
            manager->active_io_threads_num++;
        }
    }
}