
#include "task.h"
#include "zmalloc/zmalloc.h"
#include <pthread.h>
#include <inttypes.h>
#include <signal.h>
#include <errno.h>
#include <stdarg.h>
#include "anet/anet.h"

/**
 * @brief 创建一个线程任务（可变参数版本）
 * 分配 latteThreadJob 结构体，复制执行函数、回调和参数列表。
 * @param tfn       任务执行函数指针
 * @param cb        任务完成回调（可为 NULL）
 * @param arg_count 可变参数数量
 * @param ...       任务参数列表（void* 类型）
 * @return latteThreadJob* 新建任务指针
 */
latteThreadJob* createThreadJob(task_fn tfn, callback_fn cb, int arg_count, ...) {
    va_list valist;
    /* 分配任务结构体和参数数组 */
    latteThreadJob *job = zmalloc(sizeof(latteThreadJob));
    job->exec = tfn;
    job->cb = cb;
    job->argv = arg_count;
    job->args = zmalloc(sizeof(void*) * arg_count);
    /* 遍历可变参数，填充 args 数组 */
    va_start(valist, arg_count);
    for (int i = 0; i < arg_count; i++) {
        job->args[i] = va_arg(valist, void *);
    }
    va_end(valist);
    return job;
}

/**
 * @brief 释放一个线程任务结构体（内部辅助函数）
 * @param job 要释放的任务指针
 */
void releaseThreadJob(latteThreadJob* job) {
    zfree(job->args); /* 先释放参数数组 */
    zfree(job);
}

/**
 * @brief 处理工作线程完成任务后的回调通知（在主线程中执行）
 * 遍历 recv_queue 中的已完成任务，依次调用回调并释放任务。
 * @param thread 目标工作线程指针
 */
void notifyCallbackHandler(latteThread* thread) {
    /* 取出 recv_queue 中的第一个已完成任务 */
    list_node_t *ln = list_first(thread->recv_queue);
    while (ln != NULL) {
        struct latteThreadJob *job = ln->value;
        /* 若任务有回调，则执行回调（在主线程/事件循环中） */
        if (job->cb != NULL) {
            job->cb(job);
        }
        /* 加锁后从队列移除任务节点 */
        pthread_mutex_lock(&thread->mutex);
        list_del_node(thread->recv_queue,ln);
        pthread_mutex_unlock(&thread->mutex);
        releaseThreadJob(job); /* 释放任务内存 */
        ln = list_first(thread->recv_queue); /* 继续处理下一个 */
    }
}

/**
 * @brief 事件循环中管道可读事件的处理函数
 * 从通知管道读取字节，根据字节内容分发到对应处理函数。
 * 'x' 表示任务完成通知，触发 notifyCallbackHandler。
 * @param el       事件循环指针（未使用）
 * @param fd       可读的管道读端 fd
 * @param privdata 工作线程指针（latteThread*）
 * @param mask     事件掩码（未使用）
 */
void taskCallbackHandler(aeEventLoop *el, int fd, void* privdata, int  mask) {
    char notify_recv_buf[512];
    int nread = read(fd, notify_recv_buf, sizeof(notify_recv_buf));
    if (nread == 0) {
        printf("[rocks] notify recv fd closed.\n");
    } else if (nread < 0) {
        printf("[rocks] read notify failed: %s\n", strerror(errno));
    } else {
        switch (notify_recv_buf[0]) {
            case 'x': /* 任务完成通知 */
                notifyCallbackHandler(privdata);
                break;
        }
    }
}

/**
 * @brief 释放单个工作线程的所有资源
 * 销毁互斥锁和条件变量，删除任务队列，关闭通知管道。
 * @param thread 目标工作线程指针
 */
void releaseOneTaskThread(latteThread* thread) {
    pthread_mutex_destroy(&thread->mutex);
    pthread_cond_destroy(&thread->job_cond);
    pthread_cond_destroy(&thread->step_cond);
    if (thread->send_queue != NULL) {
        list_delete(thread->send_queue);
    }
    if (thread->recv_queue != NULL) {
        list_delete(thread->recv_queue);
    }
    close(thread->notify_recv_fd);
    close(thread->notify_send_fd);
}

/**
 * @brief 初始化单个工作线程结构体
 * 初始化互斥锁、条件变量、任务队列，创建通知管道并设为非阻塞，
 * 将管道读端注册到事件循环的可读事件。
 * @param id     线程序号
 * @param thread 目标工作线程指针
 * @param el     事件循环指针（可为 NULL，NULL 时不注册事件）
 * @return int 成功返回 1，失败返回 0
 */
int taskThreadInit(int id, latteThread* thread, aeEventLoop* el) {
    pthread_mutex_init(&thread->mutex, NULL);
    pthread_cond_init(&thread->job_cond, NULL);
    pthread_cond_init(&thread->step_cond, NULL);
    thread->send_queue = list_new();
    thread->recv_queue = list_new();
    thread->pending = 0;
    thread->tid = id;
    /* 创建通知管道：fds[0]=读端，fds[1]=写端 */
    int fds[2];
    if (pipe(fds)) {
        printf("can't create pipe");
        return 0;
    }
    thread->notify_recv_fd = fds[0]; /* 主线程监听 */
    thread->notify_send_fd = fds[1]; /* 工作线程写入 */
    char anetErr[256];
    /* 设置管道为非阻塞，避免读写阻塞主线程或工作线程 */
    if (anetNonBlock(anetErr, thread->notify_recv_fd) != 0) {
        printf("Fatal: set notify_recv_fd non-blocking failed: %s", anetErr);
        return 0;
    }
    if (anetNonBlock(anetErr, thread->notify_send_fd) != 0) {
        printf("Fatal: set notify_recv_fd non-blocking failed: %s", anetErr);
        return 0;
    }
    /* 将管道读端注册到事件循环，任务完成时触发 taskCallbackHandler */
    if (el != NULL &&
        aeCreateFileEvent(el, thread->notify_recv_fd,
                AE_READABLE, taskCallbackHandler, thread) == AE_ERR) {
        printf("Fatal: create notify recv event failed: %s", strerror(errno));
        return 0;
    }
    return 1;
}


/**
 * @brief 创建线程池并初始化所有工作线程
 * 分配 taskThread 结构体及 tnum 个 latteThread 数组，
 * 依次调用 taskThreadInit 初始化每个工作线程。
 * @param tnum 工作线程数量
 * @param el   事件循环指针（注册通知回调用）
 * @return taskThread* 成功返回线程池指针，任意线程初始化失败返回 NULL
 */
taskThread* createTaskThread(int tnum, aeEventLoop* el) {
    taskThread* t = zmalloc(sizeof(taskThread));
    t->num = tnum;
    t->status = THREAD_INIT;
    t->threads = zmalloc(sizeof(latteThread) * tnum);

    for (int j = 0; j < tnum; j++) {
        if (taskThreadInit(j, &t->threads[j], el) == 0) {
            /* TODO: 初始化失败时应释放已分配资源 */
            return NULL;
        }
    }
    return t;
}

/**
 * @brief 释放线程池及所有工作线程资源
 * @param thread 目标线程池指针
 */
void releaseTaskThread(taskThread* thread) {
    for (int j = 0; j < thread->num; j++) {
        releaseOneTaskThread(&thread->threads[j]);
    }
    zfree(thread->threads);
    zfree(thread);
}

/**
 * @brief 通过管道向主线程发送任务完成通知（工作线程调用）
 * 写入字节 'x' 到通知管道写端，触发事件循环中的 taskCallbackHandler。
 * EAGAIN 是非阻塞管道写满时的正常情况，不视为错误。
 * @param thread 目标工作线程指针
 * @return int 成功返回 1，失败返回 -1
 */
int notifyCallback(latteThread* thread) {
    if (write(thread->notify_send_fd, "x", 1) < 1 && errno != EAGAIN) {
        printf("notifyCallback fail\n");
        return -1;
    }
    return 1;
}

/**
 * @brief 工作线程主循环：等待任务、执行任务、通知主线程
 * 线程启动后：
 *   1. 设置 CPU 亲和性并允许被 cancel
 *   2. 阻塞 SIGALRM（watchdog 信号由主线程处理）
 *   3. 循环等待 send_queue 中的任务，执行后放入 recv_queue 并写通知
 * @param arg latteThread* 指针（由 pthread_create 传入）
 * @return void* 始终返回 NULL（无限循环，通过 pthread_cancel 终止）
 */
void *taskProcess(void *arg) {
    struct latteThreadJob *job;
    latteThread* thread = (latteThread*) arg;
    sigset_t sigset;

    latteSetCpuAffinity("");     /* 设置 CPU 亲和性（若未编译 USE_SETCPUAFFINITY 则为空操作） */
    latteMakeThreadKillable();   /* 允许异步 cancel，支持 pthread_cancel 立即终止 */

    pthread_mutex_lock(&thread->mutex);
    /* 阻塞 SIGALRM，确保 watchdog 信号只由主线程处理 */
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGALRM);
    if (pthread_sigmask(SIG_BLOCK, &sigset, NULL))
            printf("Warning: can't mask SIGALRM in bio.c thread: %s\n", strerror(errno));

    while(1) {
        list_node_t *ln;
        /* send_queue 为空时等待任务到来 */
        if (list_length(thread->send_queue) == 0) {
            pthread_cond_wait(&thread->job_cond, &thread->mutex);
            continue;
        }
        /* 取出队列头部任务 */
        ln = list_first(thread->send_queue);
        job = ln->value;

        pthread_mutex_unlock(&thread->mutex);
        job->exec(job); /* 执行任务（解锁后执行，允许并发提交新任务） */
        pthread_mutex_lock(&thread->mutex);
        /* 将完成的任务放入 recv_queue，通知主线程，并从 send_queue 移除 */
        list_add_node_tail(thread->recv_queue, job);
        notifyCallback(thread);
        list_del_node(thread->send_queue, ln);
        thread->pending--;
        pthread_cond_broadcast(&thread->step_cond); /* 唤醒等待 step_cond 的调用方 */
    }
}

/** @brief 工作线程栈大小：4MB */
#define LATTE_THREAD_STACK_SIZE (1024*1024*4)

/**
 * @brief 启动线程池中的所有工作线程
 * 设置线程栈大小（至少 4MB），为每个 latteThread 创建 POSIX 线程。
 * @param thread 目标线程池指针
 */
void startTaskThread(taskThread* thread) {
    pthread_attr_t attr;
    pthread_t t;
    size_t stacksize;

    pthread_attr_init(&attr);
    pthread_attr_getstacksize(&attr, &stacksize);
    if (!stacksize) stacksize = 1;
    /* 保证栈大小至少为 LATTE_THREAD_STACK_SIZE */
    while (stacksize < LATTE_THREAD_STACK_SIZE) stacksize *= 2;
    pthread_attr_setstacksize(&attr, stacksize);

    for (int j = 0; j < thread->num; j++) {
        if (pthread_create(&t, &attr, taskProcess, thread->threads + j) != 0) {
            printf("Fatal: Can't initialize Background Jobs.\n");
            exit(1);
        }
        thread->threads[j].thread = t;
    }
}

/**
 * @brief 停止线程池中的所有工作线程
 * 对每个工作线程发送 pthread_cancel，并 join 等待其退出。
 * 跳过与当前线程相同的 tid（防止自我 cancel）。
 * @param thread 目标线程池指针
 */
void stopTaskThread(taskThread* thread) {
    int err, j;
    for (j = 0; j < thread->num; j++) {
        latteThread* t = thread->threads + j;
        if (t->thread == pthread_self()) continue; /* 跳过当前线程 */
        if (t->thread && pthread_cancel(t->thread) == 0) {
            if ((err = pthread_join(t->thread, NULL)) != 0) {
                printf("Bio thread for job type #%d can not be joined: %s\n",
                        j, strerror(err));
            } else {
                printf("Bio thread for job type #%d terminated\n", j);
            }
        }
    }
}

/**
 * @brief 设置当前线程的 CPU 亲和性（若编译时启用 USE_SETCPUAFFINITY）
 * @param cpulist CPU 核心列表字符串（如 "0,1,2"），未启用时为空操作
 */
void latteSetCpuAffinity(const char *cpulist) {
#ifdef USE_SETCPUAFFINITY
    setcpuaffinity(cpulist);
#else
    UNUSED(cpulist);
#endif
}

/**
 * @brief 设置当前线程为可被异步 cancel 的状态
 * 将 cancelstate 设为 ENABLE，canceltype 设为 ASYNCHRONOUS，
 * 支持 pthread_cancel 在任意时刻立即终止线程。
 */
void latteMakeThreadKillable(void) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
}

/**
 * @brief 尝试将任务提交到指定工作线程（内部辅助函数）
 * 若线程积压任务数已达上限（maxJobs > 0），则返回 0 拒绝提交。
 * @param thread 线程池指针
 * @param i      目标工作线程序号
 * @param t      要提交的任务指针
 * @return int   成功返回 1，队列满返回 0
 */
int trySubmitTask(taskThread* thread, int i, latteThreadJob* t) {
    latteThread* l = thread->threads + i;
    /* maxJobs > 0 时才做积压检查，0 表示不限制 */
    if (thread->maxJobs > 0 && thread->maxJobs <= l->pending) {
        return 0;
    }
    pthread_mutex_lock(&l->mutex);
    list_add_node_tail(l->send_queue, t); /* 加入发送队列 */
    l->pending++;
    pthread_cond_signal(&l->job_cond);    /* 唤醒工作线程 */
    pthread_mutex_unlock(&l->mutex);
    return 1;
}

/**
 * @brief 以轮询方式向线程池提交任务
 * 使用静态计数器 i 轮询各线程，直到找到一个可接受任务的线程为止。
 * @param thread 目标线程池指针
 * @param t      要提交的任务指针
 */
void submitTask(taskThread* thread, latteThreadJob* t) {
    static int i = -1;
    while (1) {
        i++;
        /* 对线程数取模，实现轮询负载均衡 */
        if (trySubmitTask(thread, i % (thread->num), t)) {
            break;
        }
    }
}

