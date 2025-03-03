

#include "thread_pool.h"
#include <assert.h>
#include "log/log.h"
#include "anet/anet.h"
#include <errno.h>
#include <string.h>
#include "utils/utils.h"


void latte_thread_task_delete(latte_thread_task_t *t) {
    latte_thread_task_t* task = (latte_thread_task_t*)t;
    task->free(task);
    zfree(task);
}
void latte_thread_callback_task(latte_thread_t* latte_thread, latte_thread_task_t* task) {

    latte_mutex_lock(latte_thread->pool->callback_lock);
    list_add_node_tail(latte_thread->pool->callback_queue, task);
    latte_mutex_unlock(latte_thread->pool->callback_lock);

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
    latte_atomic_decr(latte_thread->current_tasks, 1);
    latte_thread_callback_task(latte_thread, task);
}

void* latte_thread_create(void* arg) {
    latte_thread_t* latte_thread = (latte_thread_t*)arg;
    char thdname[16];
    
    snprintf(thdname, sizeof(thdname), "swap_thd_%d", latte_thread->id);
    latte_set_thread_title(thdname);
    latte_thread_task_t* task;
    
    while(1) {
        latte_mutex_lock(latte_thread->lock);
        while (list_length(latte_thread->task_queue) == 0) {
            if (latte_thread->stop) {
                latte_mutex_unlock(latte_thread->lock);
                return NULL;
            }
            if (latte_thread->idle_time == 0) {
                latte_thread->idle_time = ustime();
            }
            pthread_cond_wait(&latte_thread->cond, &latte_thread->lock->supper);
            
        }
        latte_thread->idle_time = 0;
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
    
    
    if (aeCreateFileEvent(ae, fds[0], AE_READABLE, asyncCompleteQueueHandler, pool)  == AE_ERR ) {
        log_error(LATTE_LIB, "Fatal: create notify recv event failed: %s", strerror(errno));
        return NULL;
    }
    
    return pool;
}

void latte_thread_delete(latte_thread_t* t) {
    assert(list_length(t->task_queue) == 0);
    list_delete(t->task_queue);
    latte_mutex_delete(t->lock);
    pthread_cond_destroy(&t->cond);
    zfree(t);
}

latte_thread_t* latte_thread_new(latte_thread_pool_t* pool) {
    latte_thread_t* thread = zmalloc(sizeof(latte_thread_t));
    
    thread->lock = latte_mutex_new();
    thread->task_queue = list_new();
    thread->pool = pool;
    latte_atomic_set(thread->current_tasks, 0);
    pthread_cond_init(&thread->cond, NULL);
    thread->stop = false;
    thread->idle_time = 0;
    return thread;
}

latte_error_t* thread_pool_add_thread(latte_thread_pool_t* pool, int thread_size) {
    for(int i = 0; i < thread_size; i++) {
        latte_thread_t* thread = latte_thread_new(pool);
        if (thread == NULL) {
            return error_new(CThread, "create thread struct fail", "thread_pool_add_thread");
        }
        
        if (pthread_create(&thread->thread_id, NULL, latte_thread_create, thread)) {
            latte_thread_delete(thread);
            return error_new(CThread, "pthread_create fail", "create swap threads failed.");
        }
        vector_push(pool->threads, thread);
        log_info(LATTE_LIB, "add thread pool_index %d", pool->thread_count);
        pool->thread_count++;
    }
    return &Ok;
}

latte_thread_t* thread_pool_select_thread(latte_thread_pool_t* pool) {
    size_t i;
    if (pool->thread_count < pool->core_pool_size) {
        int pool_index = pool->thread_count;
        if (error_is_ok(thread_pool_add_thread(pool, pool->core_pool_size - pool->thread_count))) {
            log_debug(LATTE_LIB, "add thread pool_index %d", pool_index);
            return (latte_thread_t*)vector_get(pool->threads, pool_index);
        }
    }
    long core_threads_min_task_size = LONG_MAX;
    size_t core_threads_min_task_index = -1;
    latte_thread_t* core_threads_min_task_thread = NULL;
    for( i = 0; i < pool->core_pool_size; i++) {
        latte_thread_t* thread_info = vector_get(pool->threads, i);
        long current_task_size;
        latte_atomic_get(thread_info->current_tasks, current_task_size);
        if (current_task_size < core_threads_min_task_size) {
            core_threads_min_task_size = current_task_size;
            core_threads_min_task_index = i;
            core_threads_min_task_thread = thread_info;
        }
    }
    if (core_threads_min_task_size < pool->create_thread_min_work_size) {
        log_debug(LATTE_LIB, "select thread core min pool_index %d:%d", core_threads_min_task_index, core_threads_min_task_size);
        return core_threads_min_task_thread;
    }
    long other_threads_enable_used_max_task_size = -1;
    size_t other_threads_enable_used_max_task_index;
    latte_thread_t* other_threads_enable_used_max_thread = NULL;
    long other_threads_min_task_size = LONG_MAX;
    size_t other_threads_min_task_index;
    latte_thread_t* other_threads_min_task_thread = NULL;
    


    for(i = pool->core_pool_size; i < pool->thread_count; i++) {
        latte_thread_t* thread_info = vector_get(pool->threads, i);
        long current_task_size;
        latte_atomic_get(thread_info->current_tasks, current_task_size);
        if (current_task_size < pool->create_thread_min_work_size) {
            if (other_threads_enable_used_max_task_size < current_task_size) {
                //core线程已经满了之后, 优先提交到未满但是最多正在执行的线程
                other_threads_enable_used_max_task_size = current_task_size;
                other_threads_enable_used_max_task_index = i;
                other_threads_enable_used_max_thread = thread_info;
            }
        }
        if (other_threads_min_task_size > current_task_size) {
            //如果满足创建线程条件 但已经到达最多线程数时 提交给任务最少的线程
            other_threads_min_task_size = current_task_size;
            other_threads_min_task_index = i;
            other_threads_min_task_thread = thread_info;
        }
    }

    if (other_threads_enable_used_max_thread != NULL) {
        log_debug(LATTE_LIB, "select other max thread  %d:%d", other_threads_enable_used_max_task_index, other_threads_enable_used_max_task_size);
        return other_threads_enable_used_max_thread;
    }
    if (pool->thread_count < pool->max_pool_size) {
        if (error_is_ok(thread_pool_add_thread(pool, 1))) {
            log_debug(LATTE_LIB, "add other thread pool_index %d", pool->thread_count - 1);
            return vector_get(pool->threads, pool->thread_count - 1);
        } else {
            return NULL;
        }
    }
    if (core_threads_min_task_size <= other_threads_min_task_size) {
        log_debug(LATTE_LIB, "last select core thread min %d:%d", core_threads_min_task_index, core_threads_min_task_size);
        return core_threads_min_task_thread;
    } else {
        log_debug(LATTE_LIB, "last select other thread min %d:%d", other_threads_min_task_index, other_threads_min_task_size);
        return other_threads_min_task_thread;
    }
}

latte_error_t* thread_submit(latte_thread_t* thread, latte_thread_task_t* task) {
    assert(thread->stop == false);
    latte_mutex_lock(thread->lock);
    list_add_node_tail(thread->task_queue, task);
    pthread_cond_signal(&thread->cond);
    latte_mutex_unlock(thread->lock);
    latte_atomic_incr(thread->current_tasks, 1);
    return &Ok;
}

latte_error_t* thread_pool_submit(latte_thread_pool_t* pool, latte_thread_task_t* task) {
    latte_thread_t* thread = thread_pool_select_thread(pool);
    assert(thread != NULL);
    return thread_submit(thread, task);
}

latte_error_t* thread_pool_try_delete(latte_thread_pool_t* pool, int index) {
    latte_thread_t* thread = vector_get(pool->threads, index);
    assert(thread->stop == false);
    latte_mutex_lock(thread->lock);
    thread->stop = true;
    pthread_cond_signal(&thread->cond);
    latte_mutex_unlock(thread->lock);
    
    int res = pthread_join(thread->thread_id, NULL);
    if (res != 0) {
        return error_new(CThread, "pthread join fail", "");
    }

    vector_remove_at(pool->threads, index);
    pool->thread_count--;
    latte_thread_delete(thread);
    log_info(LATTE_LIB, "thread_pool_try_delete %d", index);
    return &Ok;
}

void thread_pool_try_scaling(latte_thread_pool_t* pool) {
    for(int i = pool->thread_count - 1; i >= pool->core_pool_size; i--) {
        latte_thread_t* thread = vector_get(pool->threads, i);
        if (thread->idle_time == 0) {
            continue;
        }
        if ((ustime() - thread->idle_time) > pool->remove_thread_min_kepp_alive_time) {
            thread_pool_try_delete(pool, i);
        } 
    }

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


