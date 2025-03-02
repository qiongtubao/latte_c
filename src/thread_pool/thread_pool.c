

#include "thread_pool.h"
#include <assert.h>
#include "log/log.h"
#include "anet/anet.h"
#include <errno.h>
#include <string.h>


void latte_thread_task_delete(latte_thread_task_t *t) {
    latte_thread_task_t* task = (latte_thread_task_t*)t;
    task->free(task);
    zfree(task);
}
void latte_thread_callback_task(latte_thread_t* latte_thread, latte_thread_task_t* task) {
    log_info(LATTE_LIB, "latte_thread_callback_task write");
    latte_mutex_lock(latte_thread->pool->callback_lock);
    log_info(LATTE_LIB, "callback_queue %d", list_length(latte_thread->pool->callback_queue));
    list_add_node_tail(latte_thread->pool->callback_queue, task);
    log_info(LATTE_LIB, "callback_queue111");
    latte_mutex_unlock(latte_thread->pool->callback_lock);
    log_info(LATTE_LIB, "before write");
    if (write(latte_thread->pool->callback_fd, "x", 1) < 1 && errno != EAGAIN) {
        static int fail_count = 0;
        fail_count++;
        if (fail_count  == 1 || fail_count == 2 || fail_count == 3 || fail_count % 100 == 0) {
            log_error(LATTE_LIB, "[thread_pool] callback fail count %d", fail_count);
        }
    }
}

void latte_thread_exec_task(latte_thread_t* latte_thread, latte_thread_task_t* task) {
    void* result;
    latte_error_t* error = task->exec(task->arg, &result);
    task->error = error;
    task->result = result;
    log_info(LATTE_LIB, "init cacllback queue2 %d", list_length(latte_thread->pool->callback_queue));
    
    log_info(LATTE_LIB, "before callback task");
    latte_thread_callback_task(latte_thread, task);
    log_info(LATTE_LIB, "after callback task");
}

void* latte_thread_create(void* arg) {
    latte_thread_t* latte_thread = (latte_thread_t*)arg;
    char thdname[16];
    log_error(LATTE_LIB, "latte_thread_create %p %p", latte_thread, latte_thread->pool);
    
    snprintf(thdname, sizeof(thdname), "swap_thd_%d", latte_thread->id);
    latte_set_thread_title(thdname);
    latte_thread_task_t* task;
     log_info(LATTE_LIB, "latte_thread_create %d", list_length(latte_thread->pool->callback_queue));
    
    while(1) {
        latte_mutex_lock(latte_thread->lock);
        while (list_length(latte_thread->task_queue) == 0)
            pthread_cond_wait(&latte_thread->cond, &latte_thread->lock->supper);

        task = list_lpop(latte_thread->task_queue);
        latte_mutex_unlock(latte_thread->lock);
        latte_thread_exec_task(latte_thread, task);
    }
    return NULL;
}

int asyncCompleteQueueProcess(latte_thread_pool_t* pool) {
    int processed;
    list_iterator_t li;
    list_node_t* ln;
    list_t* processing_reqs = list_new();
    // monotime process_timer = 0;
    log_info(LATTE_LIB, "asyncCompleteQueueProcess %d", list_length(pool->callback_queue));
    
    latte_mutex_lock(pool->callback_lock);
    processing_reqs->head = pool->callback_queue->head;
    processing_reqs->tail = pool->callback_queue->tail;
    processing_reqs->len = pool->callback_queue->len;
    pool->callback_queue->head = pool->callback_queue->tail = NULL;
    pool->callback_queue->len = 0;
    latte_mutex_unlock(pool->callback_lock);
    list_rewind(processing_reqs, &li);
    while((ln = list_next(&li))) {
        latte_thread_task_t* task = list_node_value(ln);
        if (task->callback != NULL)
            task->callback(task->error, task->result);
        latte_thread_task_delete(task);
        list_node_value(ln) = NULL;
    }
    processed = list_length(processing_reqs);
    list_delete(processing_reqs);
    return processed;
}

void asyncCompleteQueueHandler(aeEventLoop* ae, int fd, void *privdata, int mask) {
    char notify_recv_buf[512];

    // UNUSED(ae);
    // UNUSED(mask);

    int nread = read(fd, notify_recv_buf, sizeof(notify_recv_buf));
    if (nread == 0) {
        log_error(LATTE_LIB, "[rocks] notify recv fd closed.");
    } else if (nread < 0) {
        log_error(LATTE_LIB, "[rocks] read notify failed: %s",
                strerror(errno));
    }

    asyncCompleteQueueProcess(privdata);
}

latte_thread_pool_t* thread_pool_new(size_t core_pool_size, 
    size_t max_pool_size, 
    size_t create_thread_min_work_size,
    size_t remove_thread_min_kepp_alive_time,
    aeEventLoop* ae
    ) {
    int fds[2] ;
    if (pipe(fds)) {
        return NULL;
    }
    char anetErr[256];
    if (anetNonBlock(anetErr, fds[0]) != ANET_OK) {
        log_error(LATTE_LIB, "Fatal: set notify_recv_fd non-blocking failed: %s", anetErr);
    }
    if (anetNonBlock(anetErr, fds[1]) != ANET_OK) {
        log_error(LATTE_LIB, "Fatal: set notify_send_fd non-blocking failed: %s", anetErr);
    }
    
    latte_thread_pool_t* pool = zmalloc(sizeof(latte_thread_pool_t));
    if (pool == NULL) return NULL;
    pool->thread_count = 0;
    pool->create_thread_min_work_size = create_thread_min_work_size;
    pool->remove_thread_min_kepp_alive_time = remove_thread_min_kepp_alive_time;
    pool->max_pool_size = max_pool_size;
    pool->core_pool_size = core_pool_size;
    pool->threads = vector_new();
    pool->callback_lock = latte_mutex_new();
    pool->callback_queue = list_new();
    pool->recv_fd = fds[0];
    pool->callback_fd = fds[1];
    log_info(LATTE_LIB, "init cacllback queue %d", list_length(pool->callback_queue));
    
    
    if (aeCreateFileEvent(ae, fds[0], AE_READABLE, asyncCompleteQueueHandler, pool)  == AE_ERR ) {
        log_error(LATTE_LIB, "Fatal: create notify recv event failed: %s", strerror(errno));
        return NULL;
    }
    
    return pool;
}

void latte_thread_delete(latte_thread_t* t) {
    assert(list_length(t->task_queue) == 0);
    latte_mutex_lock(t->lock);
    list_delete(t->task_queue);
    latte_mutex_unlock(t->lock);
    zfree(t);
}

latte_thread_t* latte_thread_new(latte_thread_pool_t* pool) {
    latte_thread_t* thread = zmalloc(sizeof(latte_thread_t));
    
    thread->lock = latte_mutex_new();
    thread->task_queue = list_new();
    thread->pool = pool;
    latte_atomic_set(thread->current_tasks, 0);
    pthread_cond_init(&thread->cond, NULL);
    return thread;
}

latte_error_t* thread_pool_add_thread(latte_thread_pool_t* pool, int thread_size) {
    log_error(LATTE_LIB, "thread_pool_add_thread %d", list_length(pool->callback_queue));
    for(int i = 0; i < thread_size; i++) {
        latte_thread_t* thread = latte_thread_new(pool);
        if (thread == NULL) {
            return error_new(CThread, "create thread struct fail", "thread_pool_add_thread");
        }
        log_error(LATTE_LIB, "thread_pool_add_thread %p %p", thread, thread->pool);
    
        if (pthread_create(&thread->thread_id, NULL, latte_thread_create, thread)) {
            latte_thread_delete(thread);
            return error_new(CThread, "pthread_create fail", "create swap threads failed.");
        }
        vector_push(pool->threads, thread);
        pool->thread_count++;
    }
    return &Ok;
}

latte_thread_t* thread_pool_select_thread(latte_thread_pool_t* pool) {
    if (pool->thread_count < pool->core_pool_size) {
        int pool_index = pool->thread_count;
        log_error(LATTE_LIB, "add thread %d", pool->core_pool_size - pool->thread_count);
        if (error_is_ok(thread_pool_add_thread(pool, pool->core_pool_size - pool->thread_count))) {
            return (latte_thread_t*)vector_get(pool->threads, pool_index);
        }
    }
    size_t core_threads_min_task_size = INTMAX_MAX;
    // size_t core_threads_min_task_index = -1;
    latte_thread_t* core_threads_min_task_thread = NULL;
    for(size_t i = 0; i < pool->core_pool_size; i++) {
        latte_thread_t* thread_info = vector_get(pool->threads, i);
        size_t current_task_size;
        latte_atomic_get(thread_info->current_tasks, current_task_size);
        if (current_task_size < core_threads_min_task_size) {
            core_threads_min_task_size = current_task_size;
            // core_threads_min_task_index = i;
            core_threads_min_task_thread = thread_info;
        }
    }
    if (core_threads_min_task_size < pool->create_thread_min_work_size) {
        return core_threads_min_task_thread;
    }
    size_t other_threads_enable_used_max_task_size = 0;
    // size_t other_threads_enable_used_max_task_index = -1;
    latte_thread_t* other_threads_enable_used_max_thread = NULL;
    size_t other_threads_min_task_size = INTMAX_MAX;
    // size_t other_threads_min_task_index = -1;
    latte_thread_t* other_threads_min_task_thread = NULL;
    


    for(size_t i = pool->core_pool_size; i < pool->thread_count; i++) {
        latte_thread_t* thread_info = vector_get(pool->threads, i);
        size_t current_task_size;
        latte_atomic_get(thread_info->current_tasks, current_task_size);
        if (current_task_size < pool->create_thread_min_work_size) {
            if (other_threads_enable_used_max_task_size < current_task_size) {
                //core线程已经满了之后, 优先提交到未满但是最多正在执行的线程
                other_threads_enable_used_max_task_size = current_task_size;
                // other_threads_enable_used_max_task_index = i;
                other_threads_enable_used_max_thread = thread_info;
            }
        }
        if (other_threads_min_task_size > current_task_size) {
            //如果满足创建线程条件 但已经到达最多线程数时 提交给任务最少的线程
            other_threads_min_task_size = current_task_size;
            // other_threads_min_task_index = i;
            other_threads_min_task_thread = thread_info;
        }
    }

    if (other_threads_enable_used_max_thread != NULL) return other_threads_enable_used_max_thread;
    if (pool->thread_count < pool->max_pool_size) {
        if (error_is_ok(thread_pool_add_thread(pool, 1))) {
            return vector_get(pool->threads, pool->thread_count - 1);
        } else {
            return NULL;
        }
    }
    if (core_threads_min_task_size < other_threads_min_task_size) {
        return core_threads_min_task_thread;
    } else {
        return other_threads_min_task_thread;
    }
}

latte_error_t* thread_submit(latte_thread_t* thread, latte_thread_task_t* task) {
    latte_mutex_lock(thread->lock);
    list_add_node_tail(thread->task_queue, task);
    latte_atomic_incr(thread->current_tasks, 1);
    pthread_cond_signal(&thread->cond);
    latte_mutex_unlock(thread->lock);
    return &Ok;
}

latte_error_t* thread_pool_submit(latte_thread_pool_t* pool, latte_thread_task_t* task) {
    latte_thread_t* thread = thread_pool_select_thread(pool);
    assert(thread != NULL);
    return thread_submit(thread, task);
}





//latte_thread_task

latte_thread_task_t* latte_thread_task_new(latte_thread_exec* exec , void* arg, latte_thread_callback* callback, latte_thread_task_free* free) {
    latte_thread_task_t* task = zmalloc(sizeof(latte_thread_task_t));
    task->arg = arg;
    task->exec = exec;
    task->error = NULL;
    task->result = NULL;
    task->callback = callback;
    task->free = free;
    return task;
}


