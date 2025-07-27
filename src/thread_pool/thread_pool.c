#include "thread_pool.h"

#include "utils/utils.h"

bool is_main_thread() {
    return thread_id == -1;
}
int  get_thread_id() {
    return thread_id;
}

void set_thread_id(int tid) {
    thread_id = tid;
}



thread_pool_t* latte_thread_pool_new(
    size_t min_thread_num, 
    size_t max_thread_num, 
    latte_thread_t* (*create_thread_func)(int tid), 
    void (*delete_thread_func)(latte_thread_t* thread)
) {
    thread_pool_t* pool = zmalloc(sizeof(thread_pool_t));
    pool->threads = vector_new(sizeof(latte_thread_t*), min_thread_num);
    pool->max_thread_num = max_thread_num;
    pool->min_thread_num = min_thread_num;
    pool->create_thread_func = create_thread_func;
    pool->delete_thread_func = delete_thread_func;
    for (size_t i = 0; i < pool->min_thread_num; i++) {
       latte_assert_with_info(latte_thread_pool_add_thread(pool), "latte_thread_pool_add_thread fail");
    }
    return pool;
}
void latte_thread_pool_delete(thread_pool_t* pool) {
    for(size_t i = 0; i < vector_size(pool->threads); i++) {
        latte_thread_pool_delete_thread(pool);
    }
    vector_delete(pool->threads);
    zfree(pool);
}

bool latte_thread_pool_add_thread(thread_pool_t* pool) {
    latte_assert_with_info(vector_size(pool->threads) < pool->max_thread_num, "thread pool is full");
    latte_thread_t* thread = pool->create_thread_func(vector_size(pool->threads));
    if (thread == NULL) {
        return false;
    }
    vector_push(pool->threads, thread);
    return true;
}

bool latte_thread_pool_delete_thread(thread_pool_t* pool) {
    latte_thread_t* thread = vector_pop(pool->threads);
    pool->delete_thread_func(thread);
    return true;
}

#define UNUSED(V) ((void)V)
void latte_set_cpu_affinity(const char* cpu_list) {
    #ifdef USE_SETCPUAFFINITY
    setcpuaffinity(cpu_list);
#else
    UNUSED(cpu_list);
#endif
}


void latte_make_thread_killable() {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
}


