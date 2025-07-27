#include "test/testhelp.h"
#include "test/testassert.h"

#include "thread_pool.h"
#include "list/list.h"
#include "time/localtime.h" 
#include "log/log.h"
#include "utils/utils.h"

#define LATTE_CTX_IDLE 0
#define LATTE_CTX_RUNNING 1
#define LATTE_CTX_STOP 2

typedef struct test_thread_task_t {
    int a;
    int b;
} test_thread_task_t;

typedef struct test_thread_ctx_t {
    int status;
    time_point_t idle_time;
    list_t* queue;
    long long push_count;
    long long pop_count;
    pthread_cond_t cond;
} test_thread_ctx_t;

int add(int a, int b) {
    return a + b;
}

void* test_null(test_thread_task_t* task, int a, int b) {
    return NULL;
}

void* test_start(void* arg) {
    latte_thread_t* thread = (latte_thread_t*)arg;
    test_thread_ctx_t* ctx = (test_thread_ctx_t*)thread->ctx;
    ctx->status = LATTE_THREAD_RUNNING;
    latte_set_thread_title(thread->name);
    list_t* queue_cache = list_new();
    queue_cache->free = zfree;
    list_t* tmp = NULL;
    long long qps = 0;
    long long start_time = ustime();
    while (1) {
        pthread_mutex_lock(&thread->mutex);
        while (list_length(ctx->queue) == 0) {
            if (thread->status == LATTE_THREAD_STOP) {
                pthread_mutex_unlock(&thread->mutex);
                list_delete(queue_cache);
                return NULL;
            }
            if (ctx->idle_time == -1) {
                ctx->idle_time = ustime();
            }
            ctx->status = LATTE_CTX_IDLE;
            pthread_cond_wait(&ctx->cond, &thread->mutex);
        }   
        ctx->idle_time = -1;
        ctx->status = LATTE_CTX_RUNNING;
        tmp = ctx->queue;
        ctx->queue = queue_cache;
        queue_cache = tmp;
        pthread_mutex_unlock(&thread->mutex);
        latte_iterator_t* iterator =  list_get_latte_iterator(queue_cache, 0);
        while (latte_iterator_has_next(iterator)) {
            test_thread_task_t* task = (test_thread_task_t*)latte_iterator_next(iterator);
            test_null(task, task->a, task->b);
            ctx->pop_count++;
            if (ustime() - start_time <= 1000000) {
                qps++;
            } else {
                log_debug("test", "thread %d qps: %lld\n", thread->tid, qps);
                qps = 0;
                start_time = ustime();
            }
        }
        latte_iterator_delete(iterator);
        list_empty(queue_cache);
    }
}

latte_thread_t* create_test_thread_func(int tid) {
    latte_thread_t* thread = zmalloc(sizeof(latte_thread_t));
    thread->tid = tid;

    test_thread_ctx_t* ctx = zmalloc(sizeof(test_thread_ctx_t));
    pthread_mutex_init(&thread->mutex, NULL);
    pthread_cond_init(&ctx->cond, NULL);
    ctx->queue = list_new();
    ctx->queue->free = zfree;

    thread->ctx = ctx;
    thread->name = sds_cat_fmt(sds_empty(), "test_thread_%d", tid);
    
    if (pthread_create(&thread->thread_id, NULL, test_start, thread)) {
        log_info("test", "pthread_create fail \n");
        zfree(ctx);
        zfree(thread);
        return NULL;
    }
    log_info("test", "thread %d created\n", tid);
    return thread;
}

void delete_test_thread_func(latte_thread_t* thread) {
    latte_assert_with_info(thread->ctx != NULL, "thread->ctx is NULL");
    
    test_thread_ctx_t* ctx = (test_thread_ctx_t*)thread->ctx;
    thread->status = LATTE_THREAD_STOP;
    pthread_mutex_lock(&thread->mutex);
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&thread->mutex);
    int rest = pthread_join(thread->thread_id, NULL);
    latte_assert_with_info(rest == 0, "pthread_join fail");
    latte_assert_with_info(ctx->pop_count == ctx->push_count, "pop_count: %lld, push_count: %lld", ctx->pop_count, ctx->push_count);
    pthread_mutex_destroy(&thread->mutex);
    pthread_cond_destroy(&ctx->cond);
    list_delete(ctx->queue);
    
    zfree(ctx);
    thread->ctx = NULL;
    
    if (thread->name != NULL) {
        sds_delete(thread->name);
        thread->name = NULL;
    }

    
    
    zfree(thread);
    return;
}

void thread_pool_add_task(thread_pool_t* pool, int i, void* task) {
    latte_assert_with_info(vector_size(pool->threads) != 0, "vector_size(pool->threads) is 0");
    latte_thread_t* thread = vector_get(pool->threads, (i % vector_size(pool->threads)));
    latte_assert_with_info(thread != NULL, "thread is NULL");
    if (thread == NULL) {
        return;
    }
    test_thread_ctx_t* ctx = (test_thread_ctx_t*)thread->ctx;
    pthread_mutex_lock(&thread->mutex);
    list_add_node_tail(ctx->queue, task);
    pthread_mutex_unlock(&thread->mutex);
    pthread_cond_signal(&ctx->cond);
    ctx->push_count++;
}

int thread_pool_test() {
    time_point_t start_time = ustime();
    thread_pool_t* pool = latte_thread_pool_new(5, 10, create_test_thread_func, delete_test_thread_func); 
    for (int i = 0; i < 10000000; i++) {
        test_thread_task_t* task = zmalloc(sizeof(test_thread_task_t));
        task->a = i;
        task->b = i + 1;
        thread_pool_add_task(pool, i, task);
    }
    latte_thread_pool_delete(pool);
    time_point_t end_time = ustime();
    log_debug("test", "time used: %lld\n", end_time - start_time); 
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        log_init();
        assert(log_add_stdout("test", LOG_DEBUG) == 1);
        test_cond("about thread_pool_test function", 
            thread_pool_test() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}