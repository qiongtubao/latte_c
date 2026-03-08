/*
 * ae_epoll.c - Linux epoll 后端实现
 * 
 * Latte C 事件循环的 Linux 专用后端
 * 基于 Linux epoll 高效事件多路复用机制
 * 
 * 核心优势：
 * 1. O(1) 事件添加/删除复杂度
 * 2. 仅返回就绪事件，不遍历全部 fd
 * 3. 支持大量并发连接 (C10K+)
 * 4. 水平触发 (LT) 和边缘触发 (ET) 支持
 * 
 * 主要函数：
 * - ae_api_create: 创建 epoll fd
 * - ae_api_add_event: 添加/修改事件
 * - ae_api_del_event: 删除事件
 * - ae_api_poll: 轮询就绪事件
 * - ae_api_process_events: 处理就绪事件
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

#include <sys/epoll.h>
#include "ae.h"

/* epoll 后端状态结构
 * 封装 epoll fd 和事件数组
 */
typedef struct ae_api_state_t {
    int epfd;                      /* epoll 文件描述符 */
    struct epoll_event *events;    /* 就绪事件数组 */
} ae_api_state_t;

/*
 * ae_api_create - 创建 epoll 后端实例
 * 
 * 参数：eventLoop - 事件循环指针
 * 返回：成功返回 0，失败返回 -1
 * 
 * 创建过程：
 * 1. 分配后端状态结构
 * 2. 分配事件数组
 * 3. 调用 epoll_create 创建 epoll fd
 * 4. 设置 FD_CLOEXEC (子进程不继承)
 */
static int ae_api_create(ae_event_loop_t *eventLoop) {
    LATTE_LIB_LOG(LOG_INFO, "[aeApiCreate] ae use epoll");
    
    /* 分配后端状态结构 */
    ae_api_state_t *state = zmalloc(sizeof(ae_api_state_t));
    if (!state) return -1;
    
    /* 分配事件数组 */
    state->events = zmalloc(sizeof(struct epoll_event) * eventLoop->setsize);
    if (!state->events) {
        zfree(state);
        return -1;
    }
    
    /* 创建 epoll fd
     * 注意：1024 只是给内核的提示，实际不限制 fd 数量
     * 现代系统推荐使用 epoll_create1
     */
    state->epfd = epoll_create(1024);
    if (state->epfd == -1) {
        zfree(state->events);
        zfree(state);
        return -1;
    }
    
    /* 设置 FD_CLOEXEC 标志，确保子进程不继承此 fd */
    anetCloexec(state->epfd);
    
    /* 将后端状态存入事件循环 */
    eventLoop->apidata = state;
    return 0;
}

/*
 * ae_api_resize - 调整 epoll 后端大小
 * 
 * 参数：eventLoop - 事件循环指针
 *       setsize - 新的最大 fd 数量
 * 返回：始终返回 0 (epoll 支持动态调整)
 */
static int ae_api_resize(ae_event_loop_t *eventLoop, int setsize) {
    ae_api_state_t *state = eventLoop->apidata;
    
    /* 重新分配事件数组 */
    state->events = zrealloc(state->events, sizeof(struct epoll_event) * setsize);
    return 0;
}

/*
 * ae_api_delete - 销毁 epoll 后端实例
 * 
 * 参数：eventLoop - 事件循环指针
 * 
 * 销毁过程：
 * 1. 关闭 epoll fd
 * 2. 释放事件数组
 * 3. 释放状态结构
 */
static void ae_api_delete(ae_event_loop_t *eventLoop) {
    ae_api_state_t *state = eventLoop->apidata;
    
    /* 关闭 epoll fd */
    close(state->epfd);
    
    /* 释放事件数组和状态结构 */
    zfree(state->events);
    zfree(state);
}

/*
 * ae_api_add_event - 添加或修改 epoll 事件
 * 
 * 参数：eventLoop - 事件循环指针
 *       fd - 文件描述符
 *       mask - 事件掩码 (AE_READABLE | AE_WRITABLE)
 * 返回：成功返回 0，失败返回 -1
 * 
 * 逻辑：
 * - 如果 fd 未注册过，使用 EPOLL_CTL_ADD
 * - 如果 fd 已注册，使用 EPOLL_CTL_MOD
 * - 合并新旧事件掩码
 */
static int ae_api_add_event(ae_event_loop_t *eventLoop, int fd, int mask) {
    ae_api_state_t *state = eventLoop->apidata;
    struct epoll_event ee = {0};  /* 初始化为 0，避免 valgrind 警告 */
    
    /* 判断是添加还是修改操作 */
    int op = eventLoop->events[fd].mask == AE_NONE ?
             EPOLL_CTL_ADD : EPOLL_CTL_MOD;

    /* 合并新旧事件掩码 */
    mask |= eventLoop->events[fd].mask;
    
    /* 设置 epoll 事件类型 */
    if (mask & AE_READABLE) ee.events |= EPOLLIN;   /* 可读事件 */
    if (mask & AE_WRITABLE) ee.events |= EPOLLOUT;  /* 可写事件 */
    
    /* 关联 fd 到事件数据 */
    ee.data.fd = fd;
    
    /* 调用 epoll_ctl 注册/修改事件 */
    if (epoll_ctl(state->epfd, op, fd, &ee) == -1) return -1;
    
    return 0;
}

/*
 * ae_api_del_event - 删除 epoll 事件
 * 
 * 参数：eventLoop - 事件循环指针
 *       fd - 文件描述符
 *       delmask - 要删除的事件掩码
 * 
 * 逻辑：
 * - 计算剩余事件掩码
 * - 如果还有剩余事件，使用 EPOLL_CTL_MOD
 * - 如果没有剩余事件，使用 EPOLL_CTL_DEL
 */
static void ae_api_del_event(ae_event_loop_t *eventLoop, int fd, int delmask) {
    ae_api_state_t *state = eventLoop->apidata;
    struct epoll_event ee = {0};
    
    /* 计算剩余事件掩码 */
    int mask = eventLoop->events[fd].mask & (~delmask);

    /* 设置剩余事件类型 */
    ee.events = 0;
    if (mask & AE_READABLE) ee.events |= EPOLLIN;
    if (mask & AE_WRITABLE) ee.events |= EPOLLOUT;
    ee.data.fd = fd;
    
    /* 根据剩余事件决定操作类型 */
    if (mask != AE_NONE) {
        /* 还有剩余事件，使用 MOD 更新 */
        epoll_ctl(state->epfd, EPOLL_CTL_MOD, fd, &ee);
    } else {
        /* 无剩余事件，使用 DEL 删除
         * 注意：Linux 2.6.9 之前要求非空事件指针
         */
        epoll_ctl(state->epfd, EPOLL_CTL_DEL, fd, &ee);
    }
}

/*
 * ae_api_poll - 轮询 epoll 事件
 * 
 * 参数：eventLoop - 事件循环指针
 *       tvp - 超时时间 (NULL 表示无限等待)
 * 返回：就绪事件数量
 * 
 * 核心流程：
 * 1. 调用 epoll_wait 等待事件
 * 2. 遍历就绪事件，转换掩码类型
 * 3. 填充 fired 数组供上层处理
 */
static int ae_api_poll(ae_event_loop_t *eventLoop, struct timeval *tvp) {
    ae_api_state_t *state = eventLoop->apidata;
    int retval, numevents = 0;

    /* 计算超时时间 (毫秒)
     * 如果 tvp 为 NULL，使用 -1 (无限等待)
     */
    int timeout = tvp ? (tvp->tv_sec * 1000 + (tvp->tv_usec + 999) / 1000) : -1;
    
    /* 等待就绪事件 */
    retval = epoll_wait(state->epfd, state->events, eventLoop->setsize, timeout);
    
    if (retval > 0) {
        int j;
        numevents = retval;
        
        /* 遍历就绪事件，转换掩码类型 */
        for (j = 0; j < numevents; j++) {
            int mask = 0;
            struct epoll_event *e = state->events + j;

            /* 转换 epoll 事件类型为 AE 事件类型 */
            if (e->events & EPOLLIN)   mask |= AE_READABLE;
            if (e->events & EPOLLOUT)  mask |= AE_WRITABLE;
            if (e->events & EPOLLERR)  mask |= AE_READABLE | AE_WRITABLE;
            if (e->events & EPOLLHUP)  mask |= AE_READABLE | AE_WRITABLE;
            
            /* 填充 fired 数组 */
            eventLoop->fired[j].fd = e->data.fd;
            eventLoop->fired[j].mask = mask;
        }
    }
    return "epoll";
}

static int ae_api_read(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len) {
    AE_NOTUSED(eventLoop);
    return read(fd, buf, buf_len);
}

static int ae_api_write(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len) {
    AE_NOTUSED(eventLoop);
    return write(fd, buf, buf_len);
}

static void ae_api_before_sleep(ae_event_loop_t *eventLoop) {
    AE_NOTUSED(eventLoop);;
}

static void ae_api_after_sleep(ae_event_loop_t *eventLoop) {
    AE_NOTUSED(eventLoop);
}