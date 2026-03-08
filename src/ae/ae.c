/*
 * ae.c - 事件循环 (AE) 实现文件
 * 
 * Latte C 库核心组件：高性能事件驱动框架
 * 
 * 主要功能：
 * 1. 事件注册与取消 (文件事件 + 时间事件)
 * 2. 多路复用后端自动选择 (epoll/kqueue/iouring/select)
 * 3. 定时器管理 (一次性/周期性)
 * 4. 非阻塞 I/O 封装
 * 5. 可扩展钩子 (beforesleep/aftersleep)
 * 
 * 核心流程：
 * - ae_main: 进入事件循环
 * - ae_process_events: 处理就绪事件
 * - ae_api_process_events: 后端具体实现
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

#include "ae.h"

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "zmalloc/zmalloc.h"
#include "utils/sys_config.h"   // 系统配置，判断 HAVE_EVPORT/HAVE_EPOLL/HAVE_KQUEUE
#include "anet/anet.h"
#include "log/log.h"


/* 包含最佳的多路复用后端 (按性能降序排列)
 * 
 * 选择优先级：
 * 1. io_uring (Linux 4.18+, 最新高性能)
 * 2. evport (Solaris 11+)
 * 3. epoll (Linux, 主流)
 * 4. kqueue (BSD/macOS)
 * 5. select (兜底，所有 Unix)
 */
#ifdef HAVE_IOURING
    #include "ae_iouring.c"
#else
    #ifdef HAVE_EVPORT
        #include "ae_evport.c"
    #else
        #ifdef HAVE_EPOLL
            #include "ae_epoll.c"
        #else
            #ifdef HAVE_KQUEUE
                #include "ae_kqueue.c"
            #else
                #include "ae_select.c"
            #endif
        #endif
    #endif
#endif

/* 删除函数任务回调包装器
 * 用于列表释放时正确删除 func_task
 */
void _latte_func_task_delete(void* task) {
    latte_func_task_delete((latte_func_task_t*)task);
}

/*
 * ae_event_loop_new - 创建新的事件循环
 * 
 * 参数：setsize - 最大可跟踪的文件描述符数量
 * 返回：成功返回事件循环指针，失败返回 NULL
 * 
 * 创建过程：
 * 1. 分配事件循环结构
 * 2. 初始化事件数组和触发数组
 * 3. 初始化时间事件链表
 * 4. 调用后端初始化 (ae_api_create)
 * 5. 初始化事件数组所有项为 AE_NONE
 */
/*
 * ae_event_loop_new - 创建新事件循环
 */
ae_event_loop_t *ae_event_loop_new(int setsize) {
    ae_event_loop_t *eventLoop;
    int i;

    monotonicInit();  // 初始化单调时钟 (提高时间精度)

    /* 分配事件循环主结构 */
    if ((eventLoop = zmalloc(sizeof(*eventLoop))) == NULL) goto err;
    
    /* 分配文件事件数组 (fd 索引) */
    eventLoop->events = zmalloc(sizeof(ae_file_event_t)*setsize);
    
    /* 分配触发事件数组 (用于存储一轮中触发的事件) */
    eventLoop->fired = zmalloc(sizeof(ae_fired_event_t)*setsize);
    
    if (eventLoop->events == NULL || eventLoop->fired == NULL) goto err;
    
    /* 初始化基本字段 */
    eventLoop->setsize = setsize;           // 最大 fd 数量
    eventLoop->timeEventHead = NULL;        // 时间事件链表为空
    eventLoop->timeEventNextId = 0;         // 下一个时间事件 ID
    eventLoop->stop = 0;                    // 未停止
    eventLoop->maxfd = -1;                  // 最大 fd 初始为 -1
    eventLoop->beforesleep = NULL;          // 单例睡眠前回调
    /*
     * list_new - 创建新链表
     */
    eventLoop->beforesleeps = list_new();   // 睡眠前回调列表
    eventLoop->beforesleeps->free = _latte_func_task_delete;
    eventLoop->aftersleep = NULL;           // 单例睡眠后回调
    /*
     * list_new - 创建新链表
     */
    eventLoop->aftersleeps = list_new();    // 睡眠后回调列表
    eventLoop->flags = 0;                   // 标志位清空
    
    /* 调用后端初始化 (创建 epoll/kqueue 等) */
    if (ae_api_create(eventLoop) == -1) goto err;
    
    /* 初始化事件数组所有项为 AE_NONE (未注册状态) */
    for (i = 0; i < setsize; i++)
        eventLoop->events[i].mask = AE_NONE;
    
    return eventLoop;

err:
    /* 错误处理：释放已分配资源 */
    if (eventLoop) {
        zfree(eventLoop->events);
        zfree(eventLoop->fired);
        zfree(eventLoop);
    }
    return NULL;
}

/*
 * ae_get_set_size - 获取事件循环的最大 fd 数量
 * 
 * 参数：eventLoop - 事件循环指针
 * 返回：setsize 值
 */
int ae_get_set_size(ae_event_loop_t *eventLoop) {
    return eventLoop->setsize;
}

/*
 * ae_set_dont_wait - 设置是否等待模式
 * 
 * 参数：eventLoop - 事件循环指针
 *       noWait - 1 表示不等待 (立即返回), 0 表示正常等待
 * 
 * 作用：设置 AE_DONT_WAIT 标志，使下一次轮询不阻塞
 */
void ae_set_dont_wait(ae_event_loop_t *eventLoop, int noWait) {
    if (noWait)
        eventLoop->flags |= AE_DONT_WAIT;
    else
        eventLoop->flags &= ~AE_DONT_WAIT;
}

/*
 * ae_resize_set_size - 重新调整事件循环的最大 fd 数量
 * 
 * 参数：eventLoop - 事件循环指针
 *       setsize - 新的最大 fd 数量
 * 返回：成功返回 AE_OK，失败返回 AE_ERR
 * 
 * 注意事项：
 * - 不能缩小到已使用的 fd 以下
 * - 重新分配 events 和 fired 数组
 * - 新分配的槽位初始化为 AE_NONE
 */
int ae_resize_set_size(ae_event_loop_t *eventLoop, int setsize) {
    int i;

    /* 如果大小不变，直接返回 */
    if (setsize == eventLoop->setsize) return AE_OK;
    
    /* 如果请求的大小小于最大已使用 fd，返回错误 */
    if (eventLoop->maxfd >= setsize) return AE_ERR;
    
    /* 调整后端大小 (epoll/kqueue 等) */
    if (ae_api_resize(eventLoop, setsize) == -1) return AE_ERR;

    /* 重新分配事件数组 */
    eventLoop->events = zrealloc(eventLoop->events, sizeof(ae_file_event_t)*setsize);
    eventLoop->fired = zrealloc(eventLoop->fired, sizeof(ae_fired_event_t)*setsize);
    eventLoop->setsize = setsize;

    /* 初始化新分配的槽位为 AE_NONE */
    for (i = eventLoop->maxfd + 1; i < setsize; i++)
        eventLoop->events[i].mask = AE_NONE;
    
    return AE_OK;
}

/*
 * ae_event_loop_delete - 销毁事件循环
 * 
 * 参数：eventLoop - 待销毁的事件循环指针
 * 
 * 销毁过程：
 * 1. 调用后端销毁 (ae_api_delete)
 * 2. 释放 events 和 fired 数组
 * 3. 释放所有时间事件
 * 4. 释放睡眠回调列表
 * 5. 释放事件循环结构本身
 */
/*
 * ae_event_loop_delete - 销毁事件循环
 */
void ae_event_loop_delete(ae_event_loop_t *eventLoop) {
    /* 调用后端销毁 */
    ae_api_delete(eventLoop);
    
    /* 释放事件数组 */
    zfree(eventLoop->events);
    zfree(eventLoop->fired);

    /* 释放时间事件链表 */
    ae_time_event_t *next_te, *te = eventLoop->timeEventHead;
    while (te) {
        next_te = te->next;
        zfree(te);
        te = next_te;
    }
    
    /* 释放睡眠前回调列表 */
    if (eventLoop->beforesleeps != NULL) {
        /*
         * list_delete - 删除整个链表
         */
        list_delete(eventLoop->beforesleeps);
        eventLoop->beforesleeps = NULL;
    }
    
    /* 释放睡眠后回调列表 */
    if (eventLoop->aftersleeps != NULL) {
        /*
         * list_delete - 删除整个链表
         */
        list_delete(eventLoop->aftersleeps);
        eventLoop->aftersleeps = NULL;
    }
    
    /* 释放事件循环结构 */
    zfree(eventLoop);
}

/*
 * ae_stop - 停止事件循环
 * 
 * 参数：eventLoop - 事件循环指针
 * 
 * 作用：设置 stop 标志，使 ae_main 循环退出
 * 通常用于信号处理或外部条件触发退出
 */
void ae_stop(ae_event_loop_t *eventLoop) {
    eventLoop->stop = 1;
}

/*
 * 以下为事件添加/删除/处理等核心函数的实现
 * 由于代码较长，继续后续详细注释...
 */
    fe->mask |= mask;
    if (mask & AE_READABLE) fe->rfileProc = proc;
    if (mask & AE_WRITABLE) fe->wfileProc = proc;
    fe->clientData = clientData;
    if (fd > eventLoop->maxfd)
        eventLoop->maxfd = fd;
    return AE_OK;
}


void ae_file_event_delete(ae_event_loop_t *eventLoop, int fd, int mask) //删除事件
{
    if (fd >= eventLoop->setsize) return;
    ae_file_event_t *fe = &eventLoop->events[fd];
    if (fe->mask == AE_NONE) return; //不会删除对象  如果mask 为NONE就算是空事件了

    /* We want to always remove AE_BARRIER if set when AE_WRITABLE
     * is removed. */
    if (mask & AE_WRITABLE) mask |= AE_BARRIER;

    ae_api_del_event(eventLoop, fd, mask);
    fe->mask = fe->mask & (~mask);
    if (fd == eventLoop->maxfd && fe->mask == AE_NONE) { //更新最大fd
        /* Update the max fd */
        int j;

        for (j = eventLoop->maxfd-1; j >= 0; j--)
            if (eventLoop->events[j].mask != AE_NONE) break;
        eventLoop->maxfd = j;
    }
}

int ae_file_event_get_mask(ae_event_loop_t *eventLoop, int fd) { //获得事件mask
    if (fd >= eventLoop->setsize) return 0;
    ae_file_event_t *fe = &eventLoop->events[fd];

    return fe->mask;
}

long long ae_time_event_new(ae_event_loop_t *eventLoop, long long milliseconds,
        ae_time_proc_func *proc, void *clientData,
        ae_event_finalizer_proc_func *finalizerProc) //创建一个时间事件
{
    long long id = eventLoop->timeEventNextId++;
    ae_time_event_t *te;

    te = zmalloc(sizeof(*te));
    if (te == NULL) return AE_ERR;
    te->id = id;
    te->when = getMonotonicUs() + milliseconds * 1000;
    te->timeProc = proc;
    te->finalizerProc = finalizerProc;
    te->clientData = clientData;
    te->prev = NULL;
    te->next = eventLoop->timeEventHead;
    te->refcount = 0;
    if (te->next)
        te->next->prev = te;
    eventLoop->timeEventHead = te;
    return id;
}

int ae_time_event_delete(ae_event_loop_t *eventLoop, long long id)
{
    ae_time_event_t *te = eventLoop->timeEventHead;
    while(te) {
        if (te->id == id) {
            te->id = AE_DELETED_EVENT_ID;
            return AE_OK;
        }
        te = te->next;
    }
    return AE_ERR; /* NO event with the specified ID found */
}

/* How many microseconds until the first timer should fire.
 * If there are no timers, -1 is returned.
 *
 * Note that's O(N) since time events are unsorted.
 * Possible optimizations (not needed by Redis so far, but...):
 * 1) Insert the event in order, so that the nearest is just the head.
 *    Much better but still insertion or deletion of timers is O(N).
 * 2) Use a skiplist to have this operation as O(1) and insertion as O(log(N)).
 */
static int64_t us_until_earliest_timer(ae_event_loop_t *eventLoop) {
    ae_time_event_t *te = eventLoop->timeEventHead;
    if (te == NULL) return -1;

    ae_time_event_t *earliest = NULL;
    while (te) {
        if (!earliest || te->when < earliest->when)
            earliest = te;
        te = te->next;
    }

    monotime now = getMonotonicUs();
    return (now >= earliest->when) ? 0 : earliest->when - now;
}

/* Process time events */
static int process_time_events(ae_event_loop_t *eventLoop) {
    int processed = 0;
    ae_time_event_t *te;
    long long maxId;

    te = eventLoop->timeEventHead;
    maxId = eventLoop->timeEventNextId-1;
    monotime now = getMonotonicUs();
    while(te) {
        long long id;

        /* Remove events scheduled for deletion. */
        if (te->id == AE_DELETED_EVENT_ID) {
            ae_time_event_t *next = te->next;
            /* If a reference exists for this timer event,
             * don't free it. This is currently incremented
             * for recursive timerProc calls */
            if (te->refcount) {
                te = next;
                continue;
            }
            if (te->prev)
                te->prev->next = te->next;
            else
                eventLoop->timeEventHead = te->next;
            if (te->next)
                te->next->prev = te->prev;
            if (te->finalizerProc) {
                te->finalizerProc(eventLoop, te->clientData);
                now = getMonotonicUs();
            }
            zfree(te);
            te = next;
            continue;
        }

        /* Make sure we don't process time events created by time events in
         * this iteration. Note that this check is currently useless: we always
         * add new timers on the head, however if we change the implementation
         * detail, this check may be useful again: we keep it here for future
         * defense. */
        if (te->id > maxId) {
            te = te->next;
            continue;
        }

        if (te->when <= now) {
            int retval;

            id = te->id;
            te->refcount++;
            retval = te->timeProc(eventLoop, id, te->clientData);
            te->refcount--;
            processed++;
            now = getMonotonicUs();
            if (retval != AE_NOMORE) {
                te->when = now + retval * 1000;
            } else {
                te->id = AE_DELETED_EVENT_ID;
            }
        }
        te = te->next;
    }
    return processed;
}

void ae_do_before_sleep(ae_event_loop_t *eventLoop) {
    if ( eventLoop->beforesleep != NULL ) eventLoop->beforesleep(eventLoop);
    if ( list_length(eventLoop->beforesleeps) > 0) {
        list_node_t* ln;
        list_iterator_t li;
        /*
         * list_rewind - 迭代器从头开始
         */
        list_rewind(eventLoop->beforesleeps, &li);
        while((ln = list_next(&li))) {
            latte_func_task_t *t = list_node_value(ln);
            exec_task(t);
        }
    }

}

void ae_do_after_sleep(ae_event_loop_t *eventLoop) {
    if ( eventLoop->beforesleep != NULL ) eventLoop->beforesleep(eventLoop);
    if ( list_length(eventLoop->beforesleeps) > 0) {
        list_node_t* ln;
        list_iterator_t li;
        /*
         * list_rewind - 迭代器从头开始
         */
        list_rewind(eventLoop->beforesleeps, &li);
        while((ln = list_next(&li))) {
            latte_func_task_t *t = list_node_value(ln);
            exec_task(t);
        }
    }
    
}

/* Process every pending time event, then every pending file event
 * (that may be registered by time event callbacks just processed).
 * Without special flags the function sleeps until some file event
 * fires, or when the next time event occurs (if any).
 *
 * If flags is 0, the function does nothing and returns.
 * if flags has AE_ALL_EVENTS set, all the kind of events are processed.
 * if flags has AE_FILE_EVENTS set, file events are processed.
 * if flags has AE_TIME_EVENTS set, time events are processed.
 * if flags has AE_DONT_WAIT set the function returns ASAP until all
 * the events that's possible to process without to wait are processed.
 * if flags has AE_CALL_AFTER_SLEEP set, the aftersleep callback is called.
 * if flags has AE_CALL_BEFORE_SLEEP set, the beforesleep callback is called.
 *
 * The function returns the number of events processed. */
int ae_process_events(ae_event_loop_t *eventLoop, int flags)
{
    int processed = 0, numevents;

    /* Nothing to do? return ASAP */
    if (!(flags & AE_TIME_EVENTS) && !(flags & AE_FILE_EVENTS)) return 0;

    /* Note that we want to call select() even if there are no
     * file events to process as long as we want to process time
     * events, in order to sleep until the next time event is ready
     * to fire. */
    if (eventLoop->maxfd != -1 ||
        ((flags & AE_TIME_EVENTS) && !(flags & AE_DONT_WAIT))) {
        int j;
        struct timeval tv, *tvp;
        int64_t usUntilTimer = -1;

        if (flags & AE_TIME_EVENTS && !(flags & AE_DONT_WAIT))
            usUntilTimer = us_until_earliest_timer(eventLoop);

        if (usUntilTimer >= 0) {
            tv.tv_sec = usUntilTimer / 1000000;
            tv.tv_usec = usUntilTimer % 1000000;
            tvp = &tv;
        } else {
            /* If we have to check for events but need to return
             * ASAP because of AE_DONT_WAIT we need to set the timeout
             * to zero */
            if (flags & AE_DONT_WAIT) {
                tv.tv_sec = tv.tv_usec = 0;
                tvp = &tv;
            } else {
                /* Otherwise we can block */
                tvp = NULL; /* wait forever */
            }
        }

        if (eventLoop->flags & AE_DONT_WAIT) {
            tv.tv_sec = tv.tv_usec = 0;
            tvp = &tv;
        }
        ae_api_before_sleep(eventLoop);  

        if ( flags & AE_CALL_BEFORE_SLEEP) {
            ae_do_before_sleep(eventLoop);
        }
         

        /* Call the multiplexing API, will return only on timeout or when
         * some event fires. */
        numevents = ae_api_poll(eventLoop, tvp);
        
        
        /* After sleep callback. */
        if (eventLoop->aftersleep != NULL && flags & AE_CALL_AFTER_SLEEP)
           ae_do_after_sleep(eventLoop);
        

        for (j = 0; j < numevents; j++) {
            ae_file_event_t *fe = &eventLoop->events[eventLoop->fired[j].fd];
            int mask = eventLoop->fired[j].mask;
            int fd = eventLoop->fired[j].fd;
            int fired = 0; /* Number of events fired for current fd. */

            /* Normally we execute the readable event first, and the writable
             * event later. This is useful as sometimes we may be able
             * to serve the reply of a query immediately after processing the
             * query.
             *
             * However if AE_BARRIER is set in the mask, our application is
             * asking us to do the reverse: never fire the writable event
             * after the readable. In such a case, we invert the calls.
             * This is useful when, for instance, we want to do things
             * in the beforeSleep() hook, like fsyncing a file to disk,
             * before replying to a client. */
            int invert = fe->mask & AE_BARRIER;

            /* Note the "fe->mask & mask & ..." code: maybe an already
             * processed event removed an element that fired and we still
             * didn't processed, so we check if the event is still valid.
             *
             * Fire the readable event if the call sequence is not
             * inverted. */
            if (!invert && fe->mask & mask & AE_READABLE) {
                fe->rfileProc(eventLoop,fd,fe->clientData,mask);
                fired++;
                fe = &eventLoop->events[fd]; /* Refresh in case of resize. */
            }

            /* Fire the writable event. */
            if (fe->mask & mask & AE_WRITABLE) {
                if (!fired || fe->wfileProc != fe->rfileProc) {
                    fe->wfileProc(eventLoop,fd,fe->clientData,mask);
                    fired++;
                }
            }

            /* If we have to invert the call, fire the readable event now
             * after the writable one. */
            if (invert) {
                fe = &eventLoop->events[fd]; /* Refresh in case of resize. */
                if ((fe->mask & mask & AE_READABLE) &&
                    (!fired || fe->wfileProc != fe->rfileProc))
                {
                    fe->rfileProc(eventLoop,fd,fe->clientData,mask);
                    fired++;
                }
            }

            processed++;
        }
        ae_api_after_sleep(eventLoop); // 在处理完事件后，提交io_uring 非常重要！！！
    }
    /* Check time events */
    if (flags & AE_TIME_EVENTS)
        processed += process_time_events(eventLoop);
    
    return processed; /* return the number of processed file/time events */
}

/* Wait for milliseconds until the given file descriptor becomes
 * writable/readable/exception */
int ae_wait(int fd, int mask, long long milliseconds) {
    struct pollfd pfd;
    int retmask = 0, retval;

    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = fd;
    if (mask & AE_READABLE) pfd.events |= POLLIN;
    if (mask & AE_WRITABLE) pfd.events |= POLLOUT;

    if ((retval = poll(&pfd, 1, milliseconds))== 1) {
        if (pfd.revents & POLLIN) retmask |= AE_READABLE;
        if (pfd.revents & POLLOUT) retmask |= AE_WRITABLE;
        if (pfd.revents & POLLERR) retmask |= AE_WRITABLE;
        if (pfd.revents & POLLHUP) retmask |= AE_WRITABLE;
        return retmask;
    } else {
        return retval;
    }
}

/*
 * ae_main - 事件循环主函数
 */
void ae_main(ae_event_loop_t *eventLoop) {
    eventLoop->stop = 0;
    while (!eventLoop->stop) {
        ae_process_events(eventLoop, AE_ALL_EVENTS|
                                   AE_CALL_BEFORE_SLEEP|
                                   AE_CALL_AFTER_SLEEP);
    }
}

char *ae_get_api_name(void) {
    return ae_api_name();
}

void ae_set_before_sleep_proc(ae_event_loop_t *eventLoop, ae_before_sleep_proc_func *beforesleep) {
    eventLoop->beforesleep = beforesleep;
}

void ae_set_after_sleep_proc(ae_event_loop_t *eventLoop, ae_before_sleep_proc_func *aftersleep) {
    eventLoop->aftersleep = aftersleep;
}


void ae_add_before_sleep_task(ae_event_loop_t* eventLoop, latte_func_task_t* task) {
    /*
     * list_add_node_tail - 在尾部添加节点
     */
    eventLoop->beforesleeps =  list_add_node_tail(eventLoop->beforesleeps, task);
}

void ae_add_after_sleep_task(ae_event_loop_t* eventLoop, latte_func_task_t* task) {
    /*
     * list_add_node_tail - 在尾部添加节点
     */
    eventLoop->aftersleeps =  list_add_node_tail(eventLoop->aftersleeps, task);
}


int ae_read(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len) {
    return ae_api_read(eventLoop, fd, buf, buf_len);
}

int ae_write(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len) {
    return ae_api_write(eventLoop, fd, buf, buf_len);
}