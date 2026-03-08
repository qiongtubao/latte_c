/*
 * thread_pool.h - 线程池头文件
 * 
 * Latte C 库并发组件：通用线程池实现
 * 
 * 核心特性：
 * 1. 固定大小线程池
 * 2. 任务队列管理
 * 3. 自动调度执行
 * 4. 优雅关闭支持
 * 5. 任务回调机制
 * 
 * 使用流程：
 * 1. thread_pool_new() - 创建线程池
 * 2. thread_pool_submit() - 提交任务
 * 3. thread_pool_wait() - 等待任务完成
 * 4. thread_pool_shutdown() - 关闭线程池
 * 
 * 典型应用场景：
 * - 并发处理 IO 密集型任务
 * - 限制并发数保护资源
 * - 批量任务处理
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

#ifndef __LATTE_THREAD_POOL_H
#define __LATTE_THREAD_POOL_H

#include <stddef.h>
#include <stdint.h>

/* 任务函数类型定义 */
typedef void (*thread_pool_task_fn)(void *arg, void *result);

/* 任务结果回调 */
typedef void (*thread_pool_result_fn)(void *result, void *user_data);

/* 线程池状态 */
typedef enum {
    THREAD_POOL_RUNNING,   /* 运行中 */
    THREAD_POOL_SHUTDOWN,  /* 正在关闭 */
    THREAD_POOL_STOPPED    /* 已停止 */
} thread_pool_state_t;

/* 线程池结构 */
typedef struct thread_pool_t {
    pthread_t *threads;      /* 线程数组 */
    size_t num_threads;      /* 线程数量 */
    size_t queue_size;       /* 任务队列最大容量 */
    size_t queue_head;       /* 队列头 */
    size_t queue_tail;       /* 队列尾 */
    size_t queue_count;      /* 当前任务数 */
    int shutdown;            /* 关闭标志 */
    pthread_mutex_t mutex;   /* 互斥锁 */
    pthread_cond_t cond_in;  /* 任务入队条件变量 */
    pthread_cond_t cond_out; /* 任务出队条件变量 */
} thread_pool_t;

/* 创建线程池
 * 参数：num_threads - 线程数量
 *       queue_size - 任务队列大小
 * 返回：成功返回线程池指针，失败返回 NULL
 */
thread_pool_t* thread_pool_new(size_t num_threads, size_t queue_size);

/* 销毁线程池
 * 参数：pool - 待销毁的线程池
 * 注意：必须先确保所有任务完成或等待关闭
 */
void thread_pool_delete(thread_pool_t *pool);

/* 提交任务到线程池
 * 参数：pool - 线程池指针
 *       task_fn - 任务函数
 *       arg - 任务参数
 * 返回：成功返回 0，失败返回 -1 (队列满或已关闭)
 */
int thread_pool_submit(thread_pool_t *pool, thread_pool_task_fn task_fn, void *arg);

/* 提交任务并等待结果
 * 参数：pool - 线程池指针
 *       task_fn - 任务函数
 *       arg - 任务参数
 *       result - 结果存储指针
 * 返回：成功返回 0，失败返回 -1
 */
int thread_pool_submit_and_wait(thread_pool_t *pool, thread_pool_task_fn task_fn, 
                                void *arg, void *result);

/* 等待所有任务完成
 * 参数：pool - 线程池指针
 * 阻塞直到队列为空
 */
void thread_pool_wait(thread_pool_t *pool);

/* 优雅关闭线程池
 * 参数：pool - 线程池指针
 * 等待当前任务完成，不接受新任务
 */
void thread_pool_shutdown(thread_pool_t *pool);

/* 强制关闭线程池
 * 参数：pool - 线程池指针
 * 立即停止，可能丢弃未完成任务
 */
void thread_pool_stop(thread_pool_t *pool);

/* 获取线程池状态
 * 参数：pool - 线程池指针
 * 返回：当前状态
 */
thread_pool_state_t thread_pool_get_state(thread_pool_t *pool);

/* 获取当前任务数
 * 参数：pool - 线程池指针
 * 返回：队列中任务数量
 */
size_t thread_pool_get_pending_count(thread_pool_t *pool);

/* 获取线程池大小
 * 参数：pool - 线程池指针
 * 返回：线程数量
 */
size_t thread_pool_get_thread_count(thread_pool_t *pool);

#endif /* __LATTE_THREAD_POOL_H */
