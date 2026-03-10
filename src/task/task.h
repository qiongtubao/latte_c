/*
 * task.h - task 模块头文件
 * 
 * Latte C 库组件
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

#ifndef __LATTE_TASK_H
#define __LATTE_TASK_H

#include "../utils/atomic.h"
#include "list/list.h"
#include "ae/ae.h"

/* task_fn and callback_fn are defined in func_task/func_task.h via ae/ae.h */

typedef struct latteThreadJob {
    int argv;
    void** args;
    void* result;
    int error;
    int id;
    task_fn *exec;
    callback_fn *cb;
} latteThreadJob;


#define THREAD_INIT 0
#define THREAD_RUNNING 1

typedef struct latteThread {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t job_cond;
    pthread_cond_t step_cond;
    list_t* send_queue;
    list_t* recv_queue;
    int pending;
    int tid;
    int notify_recv_fd;
    int notify_send_fd;
} latteThread;
typedef struct taskThread {
    int num;
    latteAtomic int status;
    latteThread* threads;
    //config
    int maxJobs;
} taskThread;

taskThread* createTaskThread(int tnum, ae_event_loop_t* loop);
void releaseTaskThread(taskThread* thread);
void startTaskThread(taskThread* thread);
void stopTaskThread(taskThread* thread);
latteThreadJob* createThreadJob(task_fn tfn, callback_fn cb, int arg_count, ...);
void submitTask(taskThread* thread, latteThreadJob* t);

#endif