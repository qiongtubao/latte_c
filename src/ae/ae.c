/**
 * @file ae.c
 * @brief 事件驱动框架（Async Event Loop）实现
 *        根据平台自动选择最优多路复用后端：
 *        io_uring > evport > epoll > kqueue > select
 *        提供文件事件注册/注销、定时事件管理、主循环驱动等核心逻辑
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
#include "utils/sys_config.h"   /* 判断是否 HAVE_EVPORT / HAVE_EPOLL / HAVE_KQUEUE */
#include "anet/anet.h"
#include "log/log.h"


/* 按性能从高到低选择多路复用后端实现 */
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

/**
 * @brief latte_func_task_t 的通用释放包装，供链表 free 回调使用
 * @param task 要释放的 latte_func_task_t 指针（void* 类型）
 */
void _latte_func_task_delete(void* task) {
    latte_func_task_delete((latte_func_task_t*)task);
}

/**
 * @brief 创建事件循环
 *        分配 events/fired 数组，初始化各字段，创建多路复用后端
 *        初始化 beforesleeps/aftersleeps 任务列表
 * @param setsize 支持监听的最大 fd 数量
 * @return 新建的 ae_event_loop_t 指针；内存不足或后端创建失败返回 NULL
 */
ae_event_loop_t *ae_event_loop_new(int setsize) {
    ae_event_loop_t *eventLoop;
    int i;

    monotonicInit();  /* 初始化单调时钟精度，提高定时精度 */

    if ((eventLoop = zmalloc(sizeof(*eventLoop))) == NULL) goto err;
    eventLoop->events = zmalloc(sizeof(ae_file_event_t)*setsize);   /* fd → 事件注册数组 */
    eventLoop->fired = zmalloc(sizeof(ae_fired_event_t)*setsize);   /* 本轮就绪事件数组 */
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
    if (ae_api_create(eventLoop) == -1) goto err;
    /* 将所有 fd 槽初始化为 AE_NONE（无事件） */
    for (i = 0; i < setsize; i++)
        eventLoop->events[i].mask = AE_NONE;
    return eventLoop;

err:
    if (eventLoop) {
        zfree(eventLoop->events);
        zfree(eventLoop->fired);
        zfree(eventLoop);
    }
    return NULL;
}

/**
 * @brief 返回事件循环当前的 setsize
 * @param eventLoop 目标事件循环
 * @return 最大 fd 监听数量
 */
int ae_get_set_size(ae_event_loop_t *eventLoop) {
    return eventLoop->setsize;
}

/**
 * @brief 设置/清除 AE_DONT_WAIT 标志，控制下一轮 poll 是否阻塞
 * @param eventLoop 目标事件循环
 * @param noWait    非 0 不阻塞（超时=0）；0 允许阻塞
 */
void ae_set_dont_wait(ae_event_loop_t *eventLoop, int noWait) {
    if (noWait)
        eventLoop->flags |= AE_DONT_WAIT;
    else
        eventLoop->flags &= ~AE_DONT_WAIT;
}

/**
 * @brief 调整事件循环的 setsize（扩容或缩容 events/fired 数组）
 *        若已有 fd >= 新 setsize，则拒绝缩容
 *        新增槽位初始化为 AE_NONE
 * @param eventLoop 目标事件循环
 * @param setsize   新的最大 fd 监听数量
 * @return AE_OK 成功；AE_ERR 拒绝或后端调整失败
 */
int ae_resize_set_size(ae_event_loop_t *eventLoop, int setsize) {
    int i;

    if (setsize == eventLoop->setsize) return AE_OK;
    if (eventLoop->maxfd >= setsize) return AE_ERR;
    if (ae_api_resize(eventLoop,setsize) == -1) return AE_ERR;

    eventLoop->events = zrealloc(eventLoop->events,sizeof(ae_file_event_t)*setsize);
    eventLoop->fired = zrealloc(eventLoop->fired,sizeof(ae_fired_event_t)*setsize);
    eventLoop->setsize = setsize;

    /* 新增槽位初始化为 AE_NONE */
    for (i = eventLoop->maxfd+1; i < setsize; i++)
        eventLoop->events[i].mask = AE_NONE;
    return AE_OK;
}

/**
 * @brief 销毁事件循环
 *        依次释放：多路复用后端 → events/fired 数组
 *        → 定时事件链表 → beforesleeps/aftersleeps 列表 → eventLoop 自身
 * @param eventLoop 要销毁的事件循环
 */
void ae_event_loop_delete(ae_event_loop_t *eventLoop) {
    ae_api_delete(eventLoop);
    zfree(eventLoop->events);
    zfree(eventLoop->fired);

    /* 释放定时事件链表中的所有节点 */
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

/**
 * @brief 停止事件循环（设置 stop=1，ae_main 检测后退出）
 * @param eventLoop 目标事件循环
 */
void ae_stop(ae_event_loop_t *eventLoop) {
    eventLoop->stop = 1;
}

/**
 * @brief 注册文件事件（读/写/屏障）
 *        回调函数存储在 events[fd]，同时通知多路复用后端监听该 fd
 * @param eventLoop  目标事件循环
 * @param fd         要监听的文件描述符
 * @param mask       事件掩码（AE_READABLE | AE_WRITABLE | AE_BARRIER 的组合）
 * @param proc       事件触发时的回调
 * @param clientData 回调时透传的用户数据
 * @return AE_OK 成功；AE_ERR fd 超出范围或后端注册失败
 */
int ae_file_event_new(ae_event_loop_t *eventLoop, int fd, int mask,
        ae_file_proc_func *proc, void *clientData)
{
    /* fd 超出事件数组范围，拒绝注册 */
    if (fd >= eventLoop->setsize) {
        errno = ERANGE;
        return AE_ERR;
    }
    ae_file_event_t *fe = &eventLoop->events[fd];

    if (ae_api_add_event(eventLoop, fd, mask) == -1)
        return AE_ERR;

    fe->mask |= mask;
    if (mask & AE_READABLE) fe->rfileProc = proc;
    if (mask & AE_WRITABLE) fe->wfileProc = proc;
    fe->clientData = clientData;
    if (fd > eventLoop->maxfd)
        eventLoop->maxfd = fd;
    return AE_OK;
}

/**
 * @brief 注销文件事件
 *        从 events[fd].mask 中移除指定掩码，同时通知多路复用后端停止监听
 *        若移除后 fd == maxfd 且 mask 变为 AE_NONE，则向下扫描更新 maxfd
 * @param eventLoop 目标事件循环
 * @param fd        要注销的文件描述符
 * @param mask      要移除的事件掩码
 */
void ae_file_event_delete(ae_event_loop_t *eventLoop, int fd, int mask)
{
    if (fd >= eventLoop->setsize) return;
    ae_file_event_t *fe = &eventLoop->events[fd];
    if (fe->mask == AE_NONE) return; /* 已无事件，跳过 */

    /* 移除 AE_WRITABLE 时同时移除 AE_BARRIER */
    if (mask & AE_WRITABLE) mask |= AE_BARRIER;

    ae_api_del_event(eventLoop, fd, mask);
    fe->mask = fe->mask & (~mask);
    if (fd == eventLoop->maxfd && fe->mask == AE_NONE) {
        /* 向下扫描更新 maxfd */
        int j;
        for (j = eventLoop->maxfd-1; j >= 0; j--)
            if (eventLoop->events[j].mask != AE_NONE) break;
        eventLoop->maxfd = j;
    }
}

/**
 * @brief 获取指定 fd 当前注册的事件掩码
 * @param eventLoop 目标事件循环
 * @param fd        文件描述符
 * @return 已注册掩码；fd 超出范围返回 0
 */
int ae_file_event_get_mask(ae_event_loop_t *eventLoop, int fd) {
    if (fd >= eventLoop->setsize) return 0;
    ae_file_event_t *fe = &eventLoop->events[fd];
    return fe->mask;
}

/**
 * @brief 创建定时事件并插入链表头
 *        触发时间 = 当前单调时钟 + milliseconds * 1000 微秒
 * @param eventLoop     目标事件循环
 * @param milliseconds  首次触发延迟（毫秒）
 * @param proc          定时回调（返回 AE_NOMORE 删除事件，否则为下次触发延迟毫秒数）
 * @param clientData    回调时透传的用户数据
 * @param finalizerProc 事件被删除时的析构回调（可为 NULL）
 * @return 新事件 ID；内存分配失败返回 AE_ERR
 */
long long ae_time_event_new(ae_event_loop_t *eventLoop, long long milliseconds,
        ae_time_proc_func *proc, void *clientData,
        ae_event_finalizer_proc_func *finalizerProc)
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

/**
 * @brief 标记删除指定 ID 的定时事件（延迟删除）
 *        将 te->id 设为 AE_DELETED_EVENT_ID，在 process_time_events 时真正释放
 * @param eventLoop 目标事件循环
 * @param id        要删除的定时事件 ID
 * @return AE_OK 找到并标记；AE_ERR 未找到
 */
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
    return AE_ERR;
}

/**
 * @brief 计算距离最近一个定时事件触发还有多少微秒
 *        遍历链表找到 when 最小的节点（O(N)，定时事件通常很少）
 * @param eventLoop 目标事件循环
 * @return 剩余微秒数（>= 0）；无定时事件返回 -1；已超时返回 0
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

/**
 * @brief 处理所有到期的定时事件
 *        先清理已标记删除（AE_DELETED_EVENT_ID）且无引用的节点，
 *        再调用到期事件的 timeProc：
 *          - 返回 AE_NOMORE → 标记删除
 *          - 返回正整数    → 重新调度（now + retval * 1000 us）
 *        跳过本轮新创建的定时事件（id > maxId）以防无限循环
 * @param eventLoop 目标事件循环
 * @return 本次处理的定时事件数量
 */
static int process_time_events(ae_event_loop_t *eventLoop) {
    int processed = 0;
    ae_time_event_t *te;
    long long maxId;

    te = eventLoop->timeEventHead;
    maxId = eventLoop->timeEventNextId-1;
    monotime now = getMonotonicUs();
    while(te) {
        long long id;

        /* 清理已标记删除且引用计数为 0 的节点 */
        if (te->id == AE_DELETED_EVENT_ID) {
            ae_time_event_t *next = te->next;
            /* 有引用（递归调用中）暂不释放 */
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

        /* 跳过本轮新创建的定时事件，防止无限循环 */
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
                /* 重新调度：now + retval 毫秒 */
                te->when = now + retval * 1000;
            } else {
                te->id = AE_DELETED_EVENT_ID;
            }
        }
        te = te->next;
    }
    return processed;
}

/**
 * @brief 执行 poll 前的所有钩子
 *        先调用 beforesleep 单函数钩子（旧接口），
 *        再遍历 beforesleeps 任务列表依次执行每个 latte_func_task_t
 * @param eventLoop 目标事件循环
 */
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

/**
 * @brief 执行 poll 后的所有钩子
 *        先调用 beforesleep 单函数钩子，再遍历 beforesleeps 任务列表执行
 *        （注：当前实现与 ae_do_before_sleep 相同，字段复用 beforesleeps）
 * @param eventLoop 目标事件循环
 */
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

/**
 * @brief 单轮事件处理：先处理定时事件，再 poll 处理文件事件
 *
 * 处理逻辑：
 * 1. 计算距最近定时事件的超时时间作为 poll 的等待上限
 * 2. AE_DONT_WAIT 时超时设为 0（立即返回）
 * 3. 调用 ae_api_before_sleep（后端钩子）
 * 4. 若设置 AE_CALL_BEFORE_SLEEP 则执行 beforesleep 钩子
 * 5. 调用 ae_api_poll 等待就绪事件
 * 6. 若设置 AE_CALL_AFTER_SLEEP 则执行 aftersleep 钩子
 * 7. 遍历就绪事件，按 AE_BARRIER 决定读/写触发顺序：
 *    - 正常：先 READABLE 后 WRITABLE
 *    - 有屏障：先 WRITABLE 后 READABLE
 * 8. 调用 ae_api_after_sleep（后端钩子，如提交 io_uring）
 * 9. 处理定时事件
 *
 * @param eventLoop 目标事件循环
 * @param flags     处理标志（AE_FILE_EVENTS / AE_TIME_EVENTS / AE_DONT_WAIT 等）
 * @return 本轮处理的事件总数（文件事件 + 定时事件）
 */
int ae_process_events(ae_event_loop_t *eventLoop, int flags)
{
    int processed = 0, numevents;

    /* flags 为 0 时无事可做，直接返回 */
    if (!(flags & AE_TIME_EVENTS) && !(flags & AE_FILE_EVENTS)) return 0;

    if (eventLoop->maxfd != -1 ||
        ((flags & AE_TIME_EVENTS) && !(flags & AE_DONT_WAIT))) {
        int j;
        struct timeval tv, *tvp;
        int64_t usUntilTimer = -1;

        if (flags & AE_TIME_EVENTS && !(flags & AE_DONT_WAIT))
            usUntilTimer = us_until_earliest_timer(eventLoop);

        if (usUntilTimer >= 0) {
            /* 有定时事件，设置超时为距最近定时事件的剩余时间 */
            tv.tv_sec = usUntilTimer / 1000000;
            tv.tv_usec = usUntilTimer % 1000000;
            tvp = &tv;
        } else {
            if (flags & AE_DONT_WAIT) {
                /* DONT_WAIT：超时设为 0，立即返回 */
                tv.tv_sec = tv.tv_usec = 0;
                tvp = &tv;
            } else {
                /* 无定时事件且允许阻塞：永久等待 */
                tvp = NULL;
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

        /* 调用多路复用 API，阻塞直到超时或有事件就绪 */
        numevents = ae_api_poll(eventLoop, tvp);

        /* poll 后执行 aftersleep 钩子 */
        if (eventLoop->aftersleep != NULL && flags & AE_CALL_AFTER_SLEEP)
           ae_do_after_sleep(eventLoop);

        for (j = 0; j < numevents; j++) {
            ae_file_event_t *fe = &eventLoop->events[eventLoop->fired[j].fd];
            int mask = eventLoop->fired[j].mask;
            int fd = eventLoop->fired[j].fd;
            int fired = 0;

            /* AE_BARRIER：有屏障时反转读写触发顺序（先写后读），
             * 确保落盘操作在回复客户端之前完成 */
            int invert = fe->mask & AE_BARRIER;

            /* 正常顺序：先触发 READABLE */
            if (!invert && fe->mask & mask & AE_READABLE) {
                fe->rfileProc(eventLoop,fd,fe->clientData,mask);
                fired++;
                fe = &eventLoop->events[fd]; /* resize 后刷新指针 */
            }

            /* 触发 WRITABLE（避免同一回调重复触发） */
            if (fe->mask & mask & AE_WRITABLE) {
                if (!fired || fe->wfileProc != fe->rfileProc) {
                    fe->wfileProc(eventLoop,fd,fe->clientData,mask);
                    fired++;
                }
            }

            /* 有屏障时：最后触发 READABLE */
            if (invert) {
                fe = &eventLoop->events[fd];
                if ((fe->mask & mask & AE_READABLE) &&
                    (!fired || fe->wfileProc != fe->rfileProc))
                {
                    fe->rfileProc(eventLoop,fd,fe->clientData,mask);
                    fired++;
                }
            }

            processed++;
        }
        ae_api_after_sleep(eventLoop); /* 提交 io_uring 等后端操作，非常重要！ */
    }
    /* 处理所有到期定时事件 */
    if (flags & AE_TIME_EVENTS)
        processed += process_time_events(eventLoop);

    return processed;
}

/**
 * @brief 阻塞等待指定 fd 就绪（基于 poll，不进入事件循环）
 *        用于简单的同步等待场景
 * @param fd           要等待的文件描述符
 * @param mask         期望的事件掩码（AE_READABLE | AE_WRITABLE）
 * @param milliseconds 超时时间（毫秒）
 * @return 就绪的事件掩码；超时返回 0；失败返回负值
 */
int ae_wait(int fd, int mask, long long milliseconds) {
    struct pollfd pfd;
    int retmask = 0, retval;

    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = fd;
    if (mask & AE_READABLE) pfd.events |= POLLIN;
    if (mask & AE_WRITABLE) pfd.events |= POLLOUT;

    if ((retval = poll(&pfd, 1, milliseconds))== 1) {
        if (pfd.revents & POLLIN)  retmask |= AE_READABLE;
        if (pfd.revents & POLLOUT) retmask |= AE_WRITABLE;
        if (pfd.revents & POLLERR) retmask |= AE_WRITABLE;
        if (pfd.revents & POLLHUP) retmask |= AE_WRITABLE;
        return retmask;
    } else {
        return retval;
    }
}

/**
 * @brief 启动事件循环主循环（阻塞直到 ae_stop 设置 stop=1）
 *        每轮调用 ae_process_events 处理所有类型事件及睡眠钩子
 * @param eventLoop 目标事件循环
 */
void ae_main(ae_event_loop_t *eventLoop) {
    eventLoop->stop = 0;
    while (!eventLoop->stop) {
        ae_process_events(eventLoop, AE_ALL_EVENTS|
                                   AE_CALL_BEFORE_SLEEP|
                                   AE_CALL_AFTER_SLEEP);
    }
}

/**
 * @brief 获取当前多路复用后端名称
 * @return 后端名称字符串（如 "epoll"、"kqueue"、"select"）
 */
char *ae_get_api_name(void) {
    return ae_api_name();
}

/**
 * @brief 设置 poll 前单函数钩子（旧接口）
 * @param eventLoop   目标事件循环
 * @param beforesleep 钩子函数
 */
void ae_set_before_sleep_proc(ae_event_loop_t *eventLoop, ae_before_sleep_proc_func *beforesleep) {
    eventLoop->beforesleep = beforesleep;
}

/**
 * @brief 设置 poll 后单函数钩子（旧接口）
 * @param eventLoop  目标事件循环
 * @param aftersleep 钩子函数
 */
void ae_set_after_sleep_proc(ae_event_loop_t *eventLoop, ae_before_sleep_proc_func *aftersleep) {
    eventLoop->aftersleep = aftersleep;
}

/**
 * @brief 向 beforesleeps 列表追加一个 func_task（新接口）
 *        该任务在每轮 poll 前的 ae_do_before_sleep 中被执行
 * @param eventLoop 目标事件循环
 * @param task      要追加的 latte_func_task_t 任务
 */
void ae_add_before_sleep_task(ae_event_loop_t* eventLoop, latte_func_task_t* task) {
    eventLoop->beforesleeps = list_add_node_tail(eventLoop->beforesleeps, task);
}

/**
 * @brief 向 aftersleeps 列表追加一个 func_task（新接口）
 *        该任务在每轮 poll 后执行
 * @param eventLoop 目标事件循环
 * @param task      要追加的 latte_func_task_t 任务
 */
void ae_add_after_sleep_task(ae_event_loop_t* eventLoop, latte_func_task_t* task) {
    eventLoop->aftersleeps = list_add_node_tail(eventLoop->aftersleeps, task);
}

/**
 * @brief 通过多路复用后端异步读取数据
 * @param eventLoop 目标事件循环
 * @param fd        源文件描述符
 * @param buf       接收缓冲区
 * @param buf_len   缓冲区长度
 * @return 实际读取字节数；失败返回负值
 */
int ae_read(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len) {
    return ae_api_read(eventLoop, fd, buf, buf_len);
}

/**
 * @brief 通过多路复用后端异步写入数据
 * @param eventLoop 目标事件循环
 * @param fd        目标文件描述符
 * @param buf       待写入缓冲区
 * @param buf_len   写入字节数
 * @return 实际写入字节数；失败返回负值
 */
int ae_write(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len) {
    return ae_api_write(eventLoop, fd, buf, buf_len);
}
