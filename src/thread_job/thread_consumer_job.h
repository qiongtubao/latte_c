/**
 * @file thread_consumer_job.h
 * @brief 线程消费者任务管理接口
 *        实现主线程生产、子线程消费的无锁环形缓冲队列，
 *        并提供多线程消费者管理器（基于线程池）统一调度任务分发。
 *
 * 架构：
 *   - 主线程调用 push 写入任务（生产者）
 *   - 子线程调用 peek/remove 消费任务（消费者）
 *   - 任务参数由处理函数负责释放
 */

#ifndef __LATTE_THREAD_CONSUMER_JOB_H
#define __LATTE_THREAD_CONSUMER_JOB_H

#include "thread_job/thread_job.h"
#include "utils/atomic.h"
#include "utils/sys_config.h"
#include "thread_pool/thread_pool.h"

/**
 * @brief 无锁环形缓冲任务队列
 *        主线程（生产者）写 head，子线程（消费者）读 tail，
 *        head/tail 均对齐到缓存行以避免伪共享。
 */
typedef struct latte_job_consumer_queue_t {
    latte_job_t* ring_buffer; /**< 环形缓冲区数组（大小为 size） */
    size_t size;              /**< 环形缓冲区容量（元素数量） */
    /** 生产者写入索引（主线程独占，原子变量，缓存行对齐避免伪共享） */
    latteAtomic size_t head __attribute__((aligned(CACHE_LINE_SIZE)));
    /** 消费者读取索引（子线程独占，原子变量，缓存行对齐避免伪共享） */
    latteAtomic size_t tail __attribute__((aligned(CACHE_LINE_SIZE)));
} latte_job_consumer_queue_t;

/**
 * @brief 创建无锁环形缓冲任务队列
 * @param size 环形缓冲区容量（建议为 2 的幂次）
 * @return 新建的队列指针；失败返回 NULL
 */
latte_job_consumer_queue_t* latte_job_consumer_queue_new(size_t size);

/**
 * @brief 释放无锁环形缓冲任务队列
 * @param queue 要释放的队列指针（NULL 时安全跳过）
 */
void latte_job_consumer_queue_delete(latte_job_consumer_queue_t* queue);

/**
 * @brief 向队列尾部推入一个任务（生产者调用）
 *        队列满时行为未定义，调用前应确保队列非满
 * @param queue 目标队列
 * @param job   要推入的任务指针
 */
void latte_job_consumer_queue_push(latte_job_consumer_queue_t* queue, latte_job_t* job);

/**
 * @brief 获取队列中当前可消费的任务数量
 * @param queue 目标队列
 * @return 可消费任务数量（head - tail）
 */
size_t latte_job_consumer_queue_available(latte_job_consumer_queue_t* queue);

/**
 * @brief 查看队列头部任务（不移除，消费者调用）
 *        将队列头部任务数据复制到 job 中
 * @param queue 目标队列
 * @param job   输出：头部任务数据的接收指针
 */
void latte_job_consumer_queue_peek(latte_job_consumer_queue_t* queue, latte_job_t* job);

/**
 * @brief 移除队列头部任务（消费者调用，配合 peek 使用）
 *        推进消费者索引（tail++）
 * @param queue 目标队列
 */
void latte_job_consumer_queue_remove(latte_job_consumer_queue_t* queue);

/**
 * @brief 判断队列是否为空
 * @param queue 目标队列
 * @return 队列为空返回 true；否则返回 false
 */
bool latte_job_consumer_queue_is_empty(latte_job_consumer_queue_t* queue);

/**
 * @brief 判断队列是否已满
 * @param queue 目标队列
 * @return 队列已满返回 true；否则返回 false
 */
bool latte_job_consumer_queue_is_full(latte_job_consumer_queue_t* queue);

/**
 * @brief 多线程消费者任务管理器
 *        封装线程池，统一管理多个消费者线程的调度与任务分发
 */
typedef struct latte_thread_consumer_job_manager_t {
    thread_pool_t* pool;           /**< 底层线程池 */
    size_t active_io_threads_num;  /**< 当前活跃的 IO 线程数量 */
    long long push_count;          /**< 累计推入任务计数（用于负载均衡） */
} latte_thread_consumer_job_manager_t;

/**
 * @brief 创建多线程消费者任务管理器
 * @param min_thread_num 最小线程数量
 * @param max_thread_num 最大线程数量
 * @return 新建的管理器指针；失败返回 NULL
 */
latte_thread_consumer_job_manager_t* latte_thread_consumer_job_manager_new(size_t min_thread_num, size_t max_thread_num);

/**
 * @brief 释放多线程消费者任务管理器及其线程池
 * @param manager 要释放的管理器指针（NULL 时安全跳过）
 */
void latte_thread_consumer_job_manager_delete(latte_thread_consumer_job_manager_t* manager);

/**
 * @brief 向管理器推入一个任务（按负载均衡分配给消费者线程）
 * @param manager 目标管理器
 * @param job     要推入的任务指针
 */
void latte_thread_consumer_job_manager_push(latte_thread_consumer_job_manager_t* manager, latte_job_t* job);

/**
 * @brief 动态调整管理器中的消费者线程数量
 * @param manager           目标管理器
 * @param target_thread_num 目标线程数量（不超过 max_thread_num）
 */
void latte_thread_consumer_job_manager_adjust(latte_thread_consumer_job_manager_t* manager, size_t target_thread_num);

#endif
