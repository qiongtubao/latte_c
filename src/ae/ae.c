

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
#include "utils/sys_config.h"   //这里判断是否HAVE_EVPORT,HAVE_EPOLL,HAVE_KQUEUE
#include "anet/anet.h"
#include "log/log.h"


/* Include the best multiplexing layer supported by this system.
 * The following should be ordered by performances, descending. */
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

void _latte_func_task_delete(void* task) {
    latte_func_task_delete((latte_func_task_t*)task);
}

/* create a new event loop */
ae_event_loop_t *ae_event_loop_new(int setsize) { //创建一个事件循环
    ae_event_loop_t *eventLoop;
    int i;

    monotonicInit();  //提高时间精度  /* just in case the calling app didn't initialize */

    if ((eventLoop = zmalloc(sizeof(*eventLoop))) == NULL) goto err;
    eventLoop->events = zmalloc(sizeof(ae_file_event_t)*setsize);   //创建一个事件数组
    eventLoop->fired = zmalloc(sizeof(ae_fired_event_t)*setsize);   //创建一个触发事件的数组
    if (eventLoop->events == NULL || eventLoop->fired == NULL) goto err;
    eventLoop->setsize = setsize;
    eventLoop->timeEventHead = NULL;
    eventLoop->timeEventNextId = 0;
    eventLoop->stop = 0;
    eventLoop->maxfd = -1;
    eventLoop->beforesleep = NULL;
    eventLoop->beforesleeps = list_new();
    eventLoop->beforesleeps->free = _latte_func_task_delete;
    eventLoop->aftersleep = NULL;
    eventLoop->aftersleeps = list_new();
    eventLoop->flags = 0;
    if (ae_api_create(eventLoop) == -1) goto err;   //创建一个事件循环
    /* Events with mask == AE_NONE are not set. So let's initialize the
     * vector with it. */
    for (i = 0; i < setsize; i++)
        eventLoop->events[i].mask = AE_NONE;   //初始化事件数组
    return eventLoop;

err:
    if (eventLoop) {
        zfree(eventLoop->events);
        zfree(eventLoop->fired);
        zfree(eventLoop);
    }
    return NULL;
}

/* Return the current set size. */
int ae_get_set_size(ae_event_loop_t *eventLoop) { //返回当前事件循环的设置大小
    return eventLoop->setsize;
}

/* Tells the next iteration/s of the event processing to set timeout of 0. */
void ae_set_dont_wait(ae_event_loop_t *eventLoop, int noWait) { //告诉下一个事件循环设置是否等待
    if (noWait)
        eventLoop->flags |= AE_DONT_WAIT;
    else
        eventLoop->flags &= ~AE_DONT_WAIT;
}

/* Resize the maximum set size of the event loop.
 * If the requested set size is smaller than the current set size, but
 * there is already a file descriptor in use that is >= the requested
 * set size minus one, AE_ERR is returned and the operation is not
 * performed at all.
 *
 * Otherwise AE_OK is returned and the operation is successful. */
int ae_resize_set_size(ae_event_loop_t *eventLoop, int setsize) { //重新fd数组大小
    int i;

    if (setsize == eventLoop->setsize) return AE_OK;
    if (eventLoop->maxfd >= setsize) return AE_ERR;
    if (ae_api_resize(eventLoop,setsize) == -1) return AE_ERR;

    eventLoop->events = zrealloc(eventLoop->events,sizeof(ae_file_event_t)*setsize);
    eventLoop->fired = zrealloc(eventLoop->fired,sizeof(ae_fired_event_t)*setsize);
    eventLoop->setsize = setsize;

    /* Make sure that if we created new slots, they are initialized with
     * an AE_NONE mask. */
    for (i = eventLoop->maxfd+1; i < setsize; i++)
        eventLoop->events[i].mask = AE_NONE;
    return AE_OK;
}

void ae_event_loop_delete(ae_event_loop_t *eventLoop) {//删除事件循环
    ae_api_delete(eventLoop);
    zfree(eventLoop->events);
    zfree(eventLoop->fired);

    /* Free the time events list. */
    ae_time_event_t *next_te, *te = eventLoop->timeEventHead;
    while (te) {
        next_te = te->next;
        zfree(te);
        te = next_te;
    }
    if (eventLoop->beforesleeps != NULL) {
        list_delete(eventLoop->beforesleeps);
        eventLoop->beforesleeps = NULL;
    }
    if (eventLoop->aftersleeps != NULL) {
        list_delete(eventLoop->aftersleeps);
        eventLoop->aftersleeps = NULL;
    }
    zfree(eventLoop);
}

void ae_stop(ae_event_loop_t *eventLoop) { //停止事件循环，配合ae_main使用
    eventLoop->stop = 1;
}

/**
 *  回调函数记录在外面的event数组里，标记着mask 读写事件
 *  添加event事件给对应的模型
 *  
 */
int ae_file_event_new(ae_event_loop_t *eventLoop, int fd, int mask, //创建一个文件事件
        ae_file_proc_func *proc, void *clientData)  //创建一个文件事件的回调函数
{
    //如果fd大于事件循环的设置大小，则返回错误
    if (fd >= eventLoop->setsize) {
        errno = ERANGE;
        return AE_ERR;
    }
    ae_file_event_t *fe = &eventLoop->events[fd];

    if (ae_api_add_event(eventLoop, fd, mask) == -1) //添加一个文件事件 
        return AE_ERR;
    
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
    eventLoop->beforesleeps =  list_add_node_tail(eventLoop->beforesleeps, task);
}

void ae_add_after_sleep_task(ae_event_loop_t* eventLoop, latte_func_task_t* task) {
    eventLoop->aftersleeps =  list_add_node_tail(eventLoop->aftersleeps, task);
}


int ae_read(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len) {
    return ae_api_read(eventLoop, fd, buf, buf_len);
}

int ae_write(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len) {
    return ae_api_write(eventLoop, fd, buf, buf_len);
}