#ifndef __LATTE_TASK_H
#define __LATTE_TASK_H

#include "../utils/atomic.h"
#include "list/list.h"
#include "ae/ae.h"

/**
 * @brief 任务执行函数类型
 * @param job 任务参数指针（通常指向 latteThreadJob）
 * @return int 执行结果，0 表示成功，非 0 表示失败
 */
typedef int task_fn(void* job);

/**
 * @brief 任务完成后的回调函数类型
 * @param job 任务参数指针，用于在回调中获取执行结果
 */
typedef void callback_fn(void* job);

/**
 * @brief 线程任务结构体
 *
 * 描述一个可提交到线程池执行的任务单元，包含输入参数、执行结果和回调。
 */
typedef struct latteThreadJob {
    int argv;           /**< 参数数量 */
    void** args;        /**< 参数列表（指针数组） */
    void* result;       /**< 任务执行结果指针 */
    int error;          /**< 任务执行错误码，0 表示成功 */
    int id;             /**< 任务 ID */
    task_fn *exec;      /**< 任务执行函数指针 */
    callback_fn *cb;    /**< 任务完成后的回调函数指针（可为 NULL） */
} latteThreadJob;

/** @brief 线程状态：初始化中 */
#define THREAD_INIT 0
/** @brief 线程状态：运行中 */
#define THREAD_RUNNING 1

/**
 * @brief 单个工作线程结构体
 *
 * 使用双队列（send_queue/recv_queue）与主线程通信，
 * 通过管道（notify_recv_fd/notify_send_fd）实现异步通知。
 */
typedef struct latteThread {
    pthread_t thread;           /**< POSIX 线程句柄 */
    pthread_mutex_t mutex;      /**< 保护队列的互斥锁 */
    pthread_cond_t job_cond;    /**< 新任务到来时的条件变量 */
    pthread_cond_t step_cond;   /**< 任务步进同步条件变量 */
    list_t* send_queue;         /**< 主线程 → 工作线程的任务队列 */
    list_t* recv_queue;         /**< 工作线程 → 主线程的结果队列 */
    int pending;                /**< 当前待处理任务数 */
    int tid;                    /**< 线程序号（从 0 开始） */
    int notify_recv_fd;         /**< 通知管道读端（事件循环监听） */
    int notify_send_fd;         /**< 通知管道写端（线程写入通知） */
} latteThread;

/**
 * @brief 线程池结构体
 *
 * 管理多个工作线程，提供任务提交和线程生命周期管理接口。
 */
typedef struct taskThread {
    int num;                        /**< 工作线程数量 */
    latteAtomic int status;         /**< 线程池状态（原子变量）：THREAD_INIT / THREAD_RUNNING */
    latteThread* threads;           /**< 工作线程数组 */
    int maxJobs;                    /**< 每个线程最大积压任务数 */
} taskThread;

/**
 * @brief 创建线程池并注册到事件循环
 * @param tnum 工作线程数量
 * @param loop 事件循环指针（用于注册完成通知回调）
 * @return taskThread* 成功返回线程池指针，失败返回 NULL
 */
taskThread* createTaskThread(int tnum, aeEventLoop* loop);

/**
 * @brief 释放线程池及其所有资源
 * @param thread 目标线程池指针
 */
void releaseTaskThread(taskThread* thread);

/**
 * @brief 启动线程池中的所有工作线程
 * @param thread 目标线程池指针
 */
void startTaskThread(taskThread* thread);

/**
 * @brief 停止线程池中的所有工作线程
 * @param thread 目标线程池指针
 */
void stopTaskThread(taskThread* thread);

/**
 * @brief 创建一个线程任务
 * @param tfn       任务执行函数
 * @param cb        任务完成回调（可为 NULL）
 * @param arg_count 可变参数数量
 * @param ...       任务参数列表
 * @return latteThreadJob* 新建任务指针，失败返回 NULL
 */
latteThreadJob* createThreadJob(task_fn tfn, callback_fn cb, int arg_count, ...);

/**
 * @brief 向线程池提交一个任务
 * @param thread 目标线程池指针
 * @param t      待提交的任务指针
 */
void submitTask(taskThread* thread, latteThreadJob* t);

#endif
