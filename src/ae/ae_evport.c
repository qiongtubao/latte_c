/**
 * @file ae_evport.c
 * @brief Solaris/Illumos event port 多路复用后端实现
 *        Event Port 是 Solaris 系统提供的高性能事件通知机制
 *        支持大规模并发连接，性能优于传统的 select/poll
 */

#include "ae.h"


#include <assert.h>
#include <errno.h>
#include <port.h>
#include <poll.h>

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
static int evport_debug = 0;

#define MAX_EVENT_BATCHSZ 512

/**
 * @brief Event Port 后端状态结构
 */
typedef struct ae_api_state_t {
    int     portfd;                             /**< event port 文件描述符 */
    uint_t  npending;                           /**< 待处理 fd 数量 */
    int     pending_fds[MAX_EVENT_BATCHSZ];     /**< 待处理 fd 数组 */
    int     pending_masks[MAX_EVENT_BATCHSZ];   /**< 待处理 fd 的事件掩码数组 */
} ae_api_state_t;

/**
 * @brief 创建 Event Port 后端
 *        初始化 event port 实例，设置待处理队列
 * @param eventLoop 目标事件循环
 * @return 0 成功；-1 失败
 */
static int ae_api_create(ae_event_loop_t *eventLoop) {
    LATTE_LIB_LOG(LOG_DEBUG, "[aeApiCreate] ae use evport");
    int i;
    ae_api_state_t *state = zmalloc(sizeof(ae_api_state_t));
    if (!state) return -1;

    /* 创建 event port */
    state->portfd = port_create();
    if (state->portfd == -1) {
        zfree(state);
        return -1;
    }
    anetCloexec(state->portfd); /* 设置 CLOEXEC 标志 */

    state->npending = 0;

    /* 初始化待处理队列 */
    for (i = 0; i < MAX_EVENT_BATCHSZ; i++) {
        state->pending_fds[i] = -1;
        state->pending_masks[i] = AE_NONE;
    }

    eventLoop->apidata = state;
    return 0;
}

/**
 * @brief 调整 Event Port 后端的 setsize（无操作）
 * @param eventLoop 目标事件循环
 * @param setsize   新的最大事件数量
 * @return 0 成功
 */
static int ae_api_resize(ae_event_loop_t *eventLoop, int setsize) {
    (void) eventLoop;
    (void) setsize;
    /* Event Port 无需调整大小 */
    return 0;
}

/**
 * @brief 销毁 Event Port 后端
 *        关闭 port fd，释放状态结构
 * @param eventLoop 目标事件循环
 */
static void ae_api_delete(ae_event_loop_t *eventLoop) {
    ae_api_state_t *state = eventLoop->apidata;

    close(state->portfd);
    zfree(state);
}

/**
 * @brief 在待处理队列中查找指定 fd
 * @param state Event Port 状态结构
 * @param fd    要查找的文件描述符
 * @return 找到返回索引；未找到返回 -1
 */
static int ae_api_lookup_pending(ae_api_state_t *state, int fd) {
    uint_t i;

    for (i = 0; i < state->npending; i++) {
        if (state->pending_fds[i] == fd)
            return (i);
    }

    return (-1);
}

/**
 * @brief 将 fd 与指定事件关联到 event port
 *        辅助函数，用于调用 port_associate
 * @param where   调用位置标识（用于调试）
 * @param portfd  event port 文件描述符
 * @param fd      要关联的文件描述符
 * @param mask    事件掩码
 * @return 0 成功；-1 失败
 */
static int ae_api_associate(const char *where, int portfd, int fd, int mask) {
    int events = 0;
    int rv, err;

    /* 将 AE 事件掩码转换为 poll 事件 */
    if (mask & AE_READABLE)
        events |= POLLIN;
    if (mask & AE_WRITABLE)
        events |= POLLOUT;

    if (evport_debug)
        fprintf(stderr, "%s: port_associate(%d, 0x%x) = ", where, fd, events);

    rv = port_associate(portfd, PORT_SOURCE_FD, fd, events,
        (void *)(uintptr_t)mask);
    err = errno;

    if (evport_debug)
        fprintf(stderr, "%d (%s)\n", rv, rv == 0 ? "no error" : strerror(err));

    if (rv == -1) {
        fprintf(stderr, "%s: port_associate: %s\n", where, strerror(err));

        if (err == EAGAIN)
            fprintf(stderr, "aeApiAssociate: event port limit exceeded.");
    }

    return rv;
}

/**
 * @brief 向 Event Port 添加文件事件
 *        将 fd 关联到 event port，或更新已存在 fd 的事件掩码
 * @param eventLoop 目标事件循环
 * @param fd        要注册的文件描述符
 * @param mask      事件掩码（AE_READABLE | AE_WRITABLE）
 * @return 0 成功；-1 失败
 */
static int ae_api_add_event(ae_event_loop_t *eventLoop, int fd, int mask) {
    ae_api_state_t *state = eventLoop->apidata;
    int fullmask, pfd;

    if (evport_debug)
        fprintf(stderr, "aeApiAddEvent: fd %d mask 0x%x\n", fd, mask);

    /*
     * 由于 port_associate 的 "events" 参数会替换现有事件，
     * 我们必须确保包含已关联的所有事件
     */
    fullmask = mask | eventLoop->events[fd].mask;
    pfd = ae_api_lookup_pending(state, fd);

    if (pfd != -1) {
        /*
         * 该 fd 最近从 aeApiPoll 返回。可以安全地假设消费者已处理该轮询事件，
         * 但为了更安全，我们简单地更新 pending_mask。
         * 当再次调用 aeApiPoll 时，fd 将按常规重新关联。
         */
        if (evport_debug)
            fprintf(stderr, "aeApiAddEvent: adding to pending fd %d\n", fd);
        state->pending_masks[pfd] |= fullmask;
        return 0;
    }

    return (ae_api_associate("aeApiAddEvent", state->portfd, fd, fullmask));
}

/**
 * @brief 从 Event Port 删除文件事件
 *        移除指定 fd 的某些事件监听，若无剩余事件则完全删除关联
 * @param eventLoop 目标事件循环
 * @param fd        要删除的文件描述符
 * @param mask      要删除的事件掩码
 */
static void ae_api_del_event(ae_event_loop_t *eventLoop, int fd, int mask) {
    ae_api_state_t *state = eventLoop->apidata;
    int fullmask, pfd;

    if (evport_debug)
        fprintf(stderr, "del fd %d mask 0x%x\n", fd, mask);

    pfd = ae_api_lookup_pending(state, fd);

    if (pfd != -1) {
        if (evport_debug)
            fprintf(stderr, "deleting event from pending fd %d\n", fd);

        /*
         * 该 fd 刚从 aeApiPoll 返回，所以当前未与 port 关联。
         * 我们只需要适当更新 pending_mask。
         */
        state->pending_masks[pfd] &= ~mask;

        if (state->pending_masks[pfd] == AE_NONE)
            state->pending_fds[pfd] = -1;

        return;
    }

    /*
     * fd 当前与 port 关联。与上面的添加情况一样，
     * 我们必须在更新关联之前查看文件描述符的完整掩码。
     * 我们依赖调用者已更新 eventLoop 中掩码的事实。
     */

    fullmask = eventLoop->events[fd].mask;
    if (fullmask == AE_NONE) {
        /*
         * 我们正在删除 *所有* 事件，使用 port_dissociate 完全删除关联。
         * 此处失败表示错误。
         */
        if (evport_debug)
            fprintf(stderr, "aeApiDelEvent: port_dissociate(%d)\n", fd);

        if (port_dissociate(state->portfd, PORT_SOURCE_FD, fd) != 0) {
            perror("aeApiDelEvent: port_dissociate");
            abort(); /* 不会返回 */
        }
    } else if (ae_api_associate("aeApiDelEvent", state->portfd, fd,
        fullmask) != 0) {
        /*
         * ENOMEM 是潜在的临时条件，但内核通常不会返回它，除非情况真的很糟糕。
         * EAGAIN 表示我们已达到资源限制，重试没有意义（与直觉相反）。
         * 所有其他错误表示错误。在任何这些情况下，我们能做的最好的就是中止。
         */
        abort(); /* 不会返回 */
    }
}

/**
 * @brief Event Port 事件轮询
 *        等待就绪事件，处理待处理队列的重新关联
 * @param eventLoop 目标事件循环
 * @param tvp       超时时间（NULL 表示永久等待）
 * @return 就绪事件数量
 */
static int ae_api_poll(ae_event_loop_t *eventLoop, struct timeval *tvp) {
    ae_api_state_t *state = eventLoop->apidata;
    struct timespec timeout, *tsp;
    uint_t mask, i;
    uint_t nevents;
    port_event_t event[MAX_EVENT_BATCHSZ];

    /*
     * 如果之前返回过 fd 事件，我们必须在调用 port_get() 之前
     * 将它们重新关联到 port。请参阅文件顶部的块注释以了解原因。
     */
    for (i = 0; i < state->npending; i++) {
        if (state->pending_fds[i] == -1)
            /* 该 fd 已被删除 */
            continue;

        if (ae_api_associate("aeApiPoll", state->portfd,
            state->pending_fds[i], state->pending_masks[i]) != 0) {
            /* 有关此情况为何致命的原因，请参阅 aeApiDelEvent */
            abort();
        }

        state->pending_masks[i] = AE_NONE;
        state->pending_fds[i] = -1;
    }

    state->npending = 0;

    /* 设置超时时间 */
    if (tvp != NULL) {
        timeout.tv_sec = tvp->tv_sec;
        timeout.tv_nsec = tvp->tv_usec * 1000;
        tsp = &timeout;
    } else {
        tsp = NULL;
    }

    /*
     * port_getn 可能在 errno == ETIME 的情况下返回一些事件！
     * 所以如果我们得到 ETIME，我们也要检查 nevents。
     */
    nevents = 1;
    if (port_getn(state->portfd, event, MAX_EVENT_BATCHSZ, &nevents,
        tsp) == -1 && (errno != ETIME || nevents == 0)) {
        if (errno == ETIME || errno == EINTR)
            return 0;

        /* 任何其他错误都表示错误 */
        perror("aeApiPoll: port_get");
        abort();
    }

    state->npending = nevents;

    /* 处理返回的事件 */
    for (i = 0; i < nevents; i++) {
            mask = 0;
            if (event[i].portev_events & POLLIN)
                mask |= AE_READABLE;
            if (event[i].portev_events & POLLOUT)
                mask |= AE_WRITABLE;

            eventLoop->fired[i].fd = event[i].portev_object;
            eventLoop->fired[i].mask = mask;

            if (evport_debug)
                fprintf(stderr, "aeApiPoll: fd %d mask 0x%x\n",
                    (int)event[i].portev_object, mask);

            state->pending_fds[i] = event[i].portev_object;
            state->pending_masks[i] = (uintptr_t)event[i].portev_user;
    }

    return nevents;
}

/**
 * @brief 获取后端名称
 * @return 返回 "evport" 字符串
 */
static char *ae_api_name(void) {
    return "evport";
}

/**
 * @brief Event Port 后端的读操作（直接调用系统 read）
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
 * @brief Event Port 后端的写操作（直接调用系统 write）
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
 * @brief Event Port 后端的睡眠前钩子（空实现）
 * @param eventLoop 事件循环（未使用）
 */
static void ae_api_before_sleep(ae_event_loop_t *eventLoop) {
    AE_NOTUSED(eventLoop);
}

/**
 * @brief Event Port 后端的睡眠后钩子（空实现）
 * @param eventLoop 事件循环（未使用）
 */
static void ae_api_after_sleep(ae_event_loop_t *eventLoop) {
    AE_NOTUSED(eventLoop);
}






