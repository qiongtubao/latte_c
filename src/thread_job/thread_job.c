/**
 * @file thread_job.c
 * @brief 线程任务数据结构实现文件
 * @details 实现了任务对象的创建、初始化和执行功能
 * @author latte_c
 * @date 2026-03-12
 */

#include "thread_job.h"

/**
 * @brief 就地初始化已分配的任务对象
 * 设置执行函数、参数和数量，将 result 清零，error 置为 COk。
 * @param job  目标任务指针
 * @param run  任务执行函数指针
 * @param argc 参数数量
 * @param args 参数指针数组
 */
void latte_job_init(latte_job_t* job, void* (*run)(int argc, void** args, latte_error_t* error), int argc, void** args) {
    job->run = run;
    job->argc = argc;
    job->args = args;
    job->result = NULL;
    job->error.code = COk;
    job->error.state = NULL;
}

/**
 * @brief 在堆上分配并初始化一个任务对象
 * @param run  任务执行函数指针
 * @param argc 参数数量
 * @param args 参数指针数组
 * @return latte_job_t* 新建任务指针
 */
latte_job_t* latte_job_new(void* (*run)(int argc, void** args, latte_error_t* error), int argc, void** args) {
    latte_job_t* job = zmalloc(sizeof(latte_job_t));
    latte_job_init(job, run, argc, args);
    return job;
}

/**
 * @brief 释放任务对象（不释放 args 中的元素）
 * @param job 目标任务指针
 */
void latte_job_delete(latte_job_t* job) {
    zfree(job);
}

/**
 * @brief 执行任务：调用 run(argc, args, &error) 并将结果写入 job->result
 * @param job 目标任务指针
 */
void latte_job_run(latte_job_t* job) {
    job->result = job->run(job->argc, job->args, &job->error);
}