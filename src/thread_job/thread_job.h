

#ifndef __LATTE_THREAD_JOB_H
#define __LATTE_THREAD_JOB_H

#include "error/error.h"
#include "utils/atomic.h"
#include "utils/sys_config.h"

/**
 * @brief 线程任务结构体
 *
 * 封装一个可在工作线程中异步执行的任务单元：
 * 包含执行函数、参数列表、执行结果和错误信息。
 */
typedef struct latte_job_t {
    void* (*run)(int argc, void** args, latte_error_t* error); /**< 任务执行函数，返回结果指针 */
    void* result;       /**< 任务执行后的返回值（由 run 函数写入） */
    int argc;           /**< 参数数量 */
    void** args;        /**< 参数指针数组 */
    latte_error_t error;/**< 执行过程中填充的错误信息 */
} latte_job_t;

/**
 * @brief 在堆上创建并初始化一个任务对象
 * @param run  任务执行函数指针
 * @param argc 参数数量
 * @param args 参数指针数组
 * @return latte_job_t* 新建任务指针
 */
latte_job_t* latte_job_new(void* (*run)(int argc, void** args, latte_error_t* error), int argc, void** args);

/**
 * @brief 就地初始化已分配的任务对象（不分配内存）
 * @param job  目标任务指针
 * @param run  任务执行函数指针
 * @param argc 参数数量
 * @param args 参数指针数组
 */
void latte_job_init(latte_job_t* job, void* (*run)(int argc, void** args, latte_error_t* error), int argc, void** args);

/**
 * @brief 释放任务对象（不释放 args 中的元素，由调用方管理）
 * @param job 目标任务指针
 */
void latte_job_delete(latte_job_t* job);

/**
 * @brief 执行任务：调用 run 函数并将返回值写入 job->result
 * @param job 目标任务指针
 */
void latte_job_run(latte_job_t* job);



#endif
