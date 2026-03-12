/**
 * @file ae_epoll.c
 * @brief Linux epoll 多路复用后端实现
 *        提供高性能的事件驱动 I/O，适用于大量并发连接场景
 *        epoll 具有 O(1) 的事件通知复杂度，比 select/poll 性能更优
 */

#include <sys/epoll.h>
#include "ae.h"

/**
 * @brief epoll 后端状态结构
 */
typedef struct ae_api_state_t {
    int epfd;                        /**< epoll 文件描述符 */
    struct epoll_event *events;      /**< 就绪事件数组 */
} ae_api_state_t;

/**
 * @brief 创建 epoll 后端
 *        初始化 epoll 实例，分配事件数组，设置 CLOEXEC 标志
 * @param eventLoop 目标事件循环
 * @return 0 成功；-1 失败
 */
static int ae_api_create(ae_event_loop_t *eventLoop) {
    LATTE_LIB_LOG(LOG_INFO, "[aeApiCreate] ae use epoll");
    ae_api_state_t *state = zmalloc(sizeof(ae_api_state_t));

    if (!state) return -1;
    /* 分配就绪事件数组 */
    state->events = zmalloc(sizeof(struct epoll_event)*eventLoop->setsize);
    if (!state->events) {
        zfree(state);
        return -1;
    }
    /* 创建 epoll 实例，1024 是内核提示值（旧接口，新版本推荐 epoll_create1） */
    state->epfd = epoll_create(1024);
    if (state->epfd == -1) {
        zfree(state->events);
        zfree(state);
        return -1;
    }
    anetCloexec(state->epfd); /* 设置 FD_CLOEXEC，防止子进程继承 epoll fd */
    eventLoop->apidata = state;
    return 0;
}

/**
 * @brief 调整 epoll 后端的 setsize
 *        重新分配就绪事件数组以适应新的容量
 * @param eventLoop 目标事件循环
 * @param setsize   新的最大事件数量
 * @return 0 成功
 */
static int ae_api_resize(ae_event_loop_t *eventLoop, int setsize) {
    ae_api_state_t *state = eventLoop->apidata;

    state->events = zrealloc(state->events, sizeof(struct epoll_event)*setsize);
    return 0;
}

/**
 * @brief 销毁 epoll 后端
 *        关闭 epoll fd，释放事件数组和状态结构
 * @param eventLoop 目标事件循环
 */
static void ae_api_delete(ae_event_loop_t *eventLoop) {
    ae_api_state_t *state = eventLoop->apidata;

    close(state->epfd);
    zfree(state->events);
    zfree(state);
}

/**
 * @brief 向 epoll 注册文件事件
 *        将 fd 添加到 epoll 监听集合，或修改已存在 fd 的事件掩码
 * @param eventLoop 目标事件循环
 * @param fd        要注册的文件描述符
 * @param mask      事件掩码（AE_READABLE | AE_WRITABLE）
 * @return 0 成功；-1 失败
 */
static int ae_api_add_event(ae_event_loop_t *eventLoop, int fd, int mask) {
    ae_api_state_t *state = eventLoop->apidata;
    struct epoll_event ee = {0}; /* 避免 valgrind 警告 */
    /* 如果 fd 已被监听，使用 MOD 操作；否则使用 ADD 操作 */
    int op = eventLoop->events[fd].mask == AE_NONE ?
            EPOLL_CTL_ADD : EPOLL_CTL_MOD;

    ee.events = 0;
    mask |= eventLoop->events[fd].mask; /* 合并已有事件 */
    if (mask & AE_READABLE) ee.events |= EPOLLIN;   /* 注册读事件 */
    if (mask & AE_WRITABLE) ee.events |= EPOLLOUT;  /* 注册写事件 */
    ee.data.fd = fd;
    if (epoll_ctl(state->epfd,op,fd,&ee) == -1) return -1; /* 注册到 epoll */
    return 0;
}

/**
 * @brief 从 epoll 删除文件事件
 *        移除指定 fd 的某些事件监听，若无剩余事件则完全删除
 * @param eventLoop 目标事件循环
 * @param fd        要删除的文件描述符
 * @param delmask   要删除的事件掩码
 */
static void ae_api_del_event(ae_event_loop_t *eventLoop, int fd, int delmask) {
    ae_api_state_t *state = eventLoop->apidata;
    struct epoll_event ee = {0}; /* 避免 valgrind 警告 */
    int mask = eventLoop->events[fd].mask & (~delmask);

    ee.events = 0;
    if (mask & AE_READABLE) ee.events |= EPOLLIN;
    if (mask & AE_WRITABLE) ee.events |= EPOLLOUT;
    ee.data.fd = fd;
    if (mask != AE_NONE) {
        /* 还有其他事件，使用 MOD 操作更新 */
        epoll_ctl(state->epfd,EPOLL_CTL_MOD,fd,&ee);
    } else {
        /* 无剩余事件，完全删除 fd
         * 注意：内核 < 2.6.9 的 EPOLL_CTL_DEL 需要非空事件指针 */
        epoll_ctl(state->epfd,EPOLL_CTL_DEL,fd,&ee);
    }
}

/**
 * @brief epoll 事件轮询
 *        等待就绪事件，将结果填充到 eventLoop->fired 数组
 * @param eventLoop 目标事件循环
 * @param tvp       超时时间（NULL 表示永久等待）
 * @return 就绪事件数量
 */
static int ae_api_poll(ae_event_loop_t *eventLoop, struct timeval *tvp) {
    ae_api_state_t *state = eventLoop->apidata;
    int retval, numevents = 0;

    /* 调用 epoll_wait，超时转换：秒*1000 + 微秒四舍五入到毫秒 */
    retval = epoll_wait(state->epfd,state->events,eventLoop->setsize,
            tvp ? (tvp->tv_sec*1000 + (tvp->tv_usec + 999)/1000) : -1);
    if (retval > 0) {
        int j;

        numevents = retval;
        for (j = 0; j < numevents; j++) {
            int mask = 0;
            struct epoll_event *e = state->events+j;

            /* 将 epoll 事件转换为 AE 事件掩码 */
            if (e->events & EPOLLIN) mask |= AE_READABLE;
            if (e->events & EPOLLOUT) mask |= AE_WRITABLE;
            if (e->events & EPOLLERR) mask |= AE_WRITABLE|AE_READABLE;
            if (e->events & EPOLLHUP) mask |= AE_WRITABLE|AE_READABLE;
            eventLoop->fired[j].fd = e->data.fd;
            eventLoop->fired[j].mask = mask;
        }
    }
    return numevents;
}

/**
 * @brief 获取后端名称
 * @return 返回 "epoll" 字符串
 */
static char *ae_api_name(void) {
    return "epoll";
}

/**
 * @brief epoll 后端的读操作（直接调用系统 read）
 * @param eventLoop 事件循环（未使用）
 * @param fd        文件描述符
 * @param buf       接收缓冲区
 * @param buf_len   缓冲区长度
 * @return 实际读取字节数
 */
static int ae_api_read(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len) {
    AE_NOTUSED(eventLoop);
    return read(fd, buf, buf_len);
}

/**
 * @brief epoll 后端的写操作（直接调用系统 write）
 * @param eventLoop 事件循环（未使用）
 * @param fd        文件描述符
 * @param buf       发送缓冲区
 * @param buf_len   写入字节数
 * @return 实际写入字节数
 */
static int ae_api_write(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len) {
    AE_NOTUSED(eventLoop);
    return write(fd, buf, buf_len);
}

/**
 * @brief epoll 后端的睡眠前钩子（空实现）
 * @param eventLoop 事件循环（未使用）
 */
static void ae_api_before_sleep(ae_event_loop_t *eventLoop) {
    AE_NOTUSED(eventLoop);;
}

/**
 * @brief epoll 后端的睡眠后钩子（空实现）
 * @param eventLoop 事件循环（未使用）
 */
static void ae_api_after_sleep(ae_event_loop_t *eventLoop) {
    AE_NOTUSED(eventLoop);
}