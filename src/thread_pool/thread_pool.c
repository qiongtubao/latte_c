/**
 * @file thread_pool.c
 * @brief 线程池管理实现文件
 * @details 实现了线程池的创建、管理和动态调整功能
 * @author latte_c
 * @date 2026-03-12
 */

#include "thread_pool.h"

#include "utils/utils.h"

/**
 * @brief 判断当前线程是否为主线程（thread_id == -1）
 * @return bool 主线程返回 true，工作线程返回 false
 */
bool is_main_thread() {
    return thread_id == -1;
}

/**
 * @brief 获取当前线程 ID（线程本地变量）
 * @return int 线程 ID，主线程为 -1
 */
int  get_thread_id() {
    return thread_id;
}

/**
 * @brief 设置当前线程 ID（线程本地变量）
 * @param tid 要设置的线程 ID
 */
void set_thread_id(int tid) {
    thread_id = tid;
}

/**
 * @brief 创建线程池并初始化最小数量的工作线程
 *
 * 分配线程池结构，创建线程指针向量，
 * 循环调用 create_thread_func 创建 min_thread_num 个线程。
 * @param min_thread_num      初始（最小）线程数
 * @param max_thread_num      最大线程数上限
 * @param create_thread_func  线程创建工厂函数（传入 tid）
 * @param delete_thread_func  线程销毁函数
 * @return thread_pool_t* 新建线程池指针
 */
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
    /* 创建最小数量的初始工作线程 */
    for (size_t i = 0; i < pool->min_thread_num; i++) {
       latte_assert_with_info(latte_thread_pool_add_thread(pool), "latte_thread_pool_add_thread fail");
    }
    return pool;
}

/**
 * @brief 销毁线程池：逐一删除所有线程，释放向量和池本身
 * @param pool 目标线程池指针
 */
void latte_thread_pool_delete(thread_pool_t* pool) {
    for(size_t i = 0; i < vector_size(pool->threads); i++) {
        latte_thread_pool_delete_thread(pool);
    }
    vector_delete(pool->threads);
    zfree(pool);
}

/**
 * @brief 向线程池追加一个新工作线程
 * 断言池未满，调用 create_thread_func 创建线程，追加到向量尾部。
 * @param pool 目标线程池指针
 * @return bool 成功返回 true，创建失败返回 false
 */
bool latte_thread_pool_add_thread(thread_pool_t* pool) {
    latte_assert_with_info(vector_size(pool->threads) < pool->max_thread_num, "thread pool is full");
    latte_thread_t* thread = pool->create_thread_func(vector_size(pool->threads));
    if (thread == NULL) {
        return false;
    }
    vector_push(pool->threads, thread);
    return true;
}

/**
 * @brief 从线程池移除并销毁最后一个工作线程（栈式缩容）
 * 从向量尾部弹出线程指针，调用 delete_thread_func 释放资源。
 * @param pool 目标线程池指针
 * @return bool 始终返回 true
 */
bool latte_thread_pool_delete_thread(thread_pool_t* pool) {
    latte_thread_t* thread = vector_pop(pool->threads);
    pool->delete_thread_func(thread);
    return true;
}

#define UNUSED(V) ((void)V)

/**
 * @brief 设置当前线程 CPU 亲和性（编译时启用 USE_SETCPUAFFINITY 才有效）
 * @param cpu_list CPU 核心列表字符串（如 "0,1,2"），未启用时为空操作
 */
void latte_set_cpu_affinity(const char* cpu_list) {
    #ifdef USE_SETCPUAFFINITY
    setcpuaffinity(cpu_list);
#else
    UNUSED(cpu_list);
#endif
}

/**
 * @brief 将当前线程设置为可被异步 cancel 的状态
 * 启用 PTHREAD_CANCEL_ENABLE + PTHREAD_CANCEL_ASYNCHRONOUS，
 * 允许 pthread_cancel 在任意时刻立即终止线程。
 */
void latte_make_thread_killable() {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
}


