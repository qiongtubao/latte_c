/**
 * @file ae_kqueue.c
 * @brief BSD/macOS kqueue 多路复用后端实现
 *        kqueue 是 BSD 系统族（FreeBSD、OpenBSD、macOS）的高性能事件通知机制
 *        支持文件、网络、定时器等多种事件类型，性能优于 select/poll
 */

#include "ae.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

/**
 * @brief kqueue 后端状态结构
 */
typedef struct ae_api_state_t {
    int kqfd;                    /**< kqueue 文件描述符 */
    struct kevent *events;       /**< 就绪事件数组 */

    /* 用于合并读写事件的事件掩码。
     * 为了减少内存消耗，我们使用 2 位存储一个事件的掩码，
     * 因此 1 字节可以存储 4 个事件的掩码。 */
    char *eventsMask;           /**< 事件掩码数组 */
} ae_api_state_t;

#define EVENT_MASK_MALLOC_SIZE(sz) (((sz) + 3) / 4)          /**< 计算掩码数组大小 */
#define EVENT_MASK_OFFSET(fd) ((fd) % 4 * 2)                 /**< 计算 fd 在字节中的位偏移 */
#define EVENT_MASK_ENCODE(fd, mask) (((mask) & 0x3) << EVENT_MASK_OFFSET(fd))  /**< 编码掩码 */

/**
 * @brief 获取指定 fd 的事件掩码
 * @param eventsMask 掩码数组
 * @param fd         文件描述符
 * @return 事件掩码
 */
static inline int get_event_mask(const char *eventsMask, int fd) {
    return (eventsMask[fd/4] >> EVENT_MASK_OFFSET(fd)) & 0x3;
}

/**
 * @brief 为指定 fd 添加事件掩码
 * @param eventsMask 掩码数组
 * @param fd         文件描述符
 * @param mask       要添加的掩码
 */
static inline void add_event_mask(char *eventsMask, int fd, int mask) {
    eventsMask[fd/4] |= EVENT_MASK_ENCODE(fd, mask);
}

/**
 * @brief 重置指定 fd 的事件掩码
 * @param eventsMask 掩码数组
 * @param fd         文件描述符
 */
static inline void reset_event_mask(char *eventsMask, int fd) {
    eventsMask[fd/4] &= ~EVENT_MASK_ENCODE(fd, 0x3);
}

/**
 * @brief 创建 kqueue 后端
 *        初始化 kqueue 实例，分配事件数组和掩码数组
 * @param eventLoop 目标事件循环
 * @return 0 成功；-1 失败
 */
static int ae_api_create(ae_event_loop_t *eventLoop) {
    LATTE_LIB_LOG(LOG_DEBUG, "[aeApiCreate] ae use kqueue");
    ae_api_state_t *state = zmalloc(sizeof(ae_api_state_t));

    if (!state) return -1;
    /* 分配就绪事件数组 */
    state->events = zmalloc(sizeof(struct kevent)*eventLoop->setsize);
    if (!state->events) {
        zfree(state);
        return -1;
    }
    /* 创建 kqueue 实例 */
    state->kqfd = kqueue();
    if (state->kqfd == -1) {
        zfree(state->events);
        zfree(state);
        return -1;
    }
    anetCloexec(state->kqfd); /* 设置 CLOEXEC 标志 */
    /* 分配并初始化事件掩码数组 */
    state->eventsMask = zmalloc(EVENT_MASK_MALLOC_SIZE(eventLoop->setsize));
    memset(state->eventsMask, 0, EVENT_MASK_MALLOC_SIZE(eventLoop->setsize));
    eventLoop->apidata = state;
    return 0;
}

/**
 * @brief 调整 kqueue 后端的 setsize
 *        重新分配事件数组和掩码数组
 * @param eventLoop 目标事件循环
 * @param setsize   新的最大事件数量
 * @return 0 成功
 */
static int ae_api_resize(ae_event_loop_t *eventLoop, int setsize) {
    ae_api_state_t *state = eventLoop->apidata;

    state->events = zrealloc(state->events, sizeof(struct kevent)*setsize);
    state->eventsMask = zrealloc(state->eventsMask, EVENT_MASK_MALLOC_SIZE(setsize));
    memset(state->eventsMask, 0, EVENT_MASK_MALLOC_SIZE(setsize));
    return 0;
}

/**
 * @brief 销毁 kqueue 后端
 *        关闭 kqueue fd，释放事件数组、掩码数组和状态结构
 * @param eventLoop 目标事件循环
 */
static void ae_api_delete(ae_event_loop_t *eventLoop) {
    ae_api_state_t *state = eventLoop->apidata;

    close(state->kqfd);
    zfree(state->events);
    zfree(state->eventsMask);
    zfree(state);
}

/**
 * @brief 向 kqueue 添加文件事件
 *        为指定 fd 注册读/写事件监听
 * @param eventLoop 目标事件循环
 * @param fd        要注册的文件描述符
 * @param mask      事件掩码（AE_READABLE | AE_WRITABLE）
 * @return 0 成功；-1 失败
 */
static int ae_api_add_event(ae_event_loop_t *eventLoop, int fd, int mask) {
    ae_api_state_t *state = eventLoop->apidata;
    struct kevent ke;

    /* 注册读事件 */
    if (mask & AE_READABLE) {
        EV_SET(&ke, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
        if (kevent(state->kqfd, &ke, 1, NULL, 0, NULL) == -1) return -1;
    }
    /* 注册写事件 */
    if (mask & AE_WRITABLE) {
        EV_SET(&ke, fd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);
        if (kevent(state->kqfd, &ke, 1, NULL, 0, NULL) == -1) return -1;
    }
    return 0;
}

/**
 * @brief 从 kqueue 删除文件事件
 *        移除指定 fd 的某些事件监听
 * @param eventLoop 目标事件循环
 * @param fd        要删除的文件描述符
 * @param delmask   要删除的事件掩码
 */
static void ae_api_del_event(ae_event_loop_t *eventLoop, int fd, int delmask) {
    ae_api_state_t *state = eventLoop->apidata;
    struct kevent ke;

    /* 删除读事件 */
    if (delmask & AE_READABLE) {
        EV_SET(&ke, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        kevent(state->kqfd, &ke, 1, NULL, 0, NULL);
    }
    /* 删除写事件 */
    if (delmask & AE_WRITABLE) {
        EV_SET(&ke, fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
        kevent(state->kqfd, &ke, 1, NULL, 0, NULL);
    }
}

/**
 * @brief kqueue 事件轮询
 *        等待就绪事件，处理读写事件合并逻辑
 * @param eventLoop 目标事件循环
 * @param tvp       超时时间（NULL 表示永久等待）
 * @return 就绪事件数量
 */
static int ae_api_poll(ae_event_loop_t *eventLoop, struct timeval *tvp) {
    ae_api_state_t *state = eventLoop->apidata;
    int retval, numevents = 0;

    /* 设置超时时间 */
    if (tvp != NULL) {
        struct timespec timeout;
        timeout.tv_sec = tvp->tv_sec;
        timeout.tv_nsec = tvp->tv_usec * 1000;
        retval = kevent(state->kqfd, NULL, 0, state->events, eventLoop->setsize,
                        &timeout);
    } else {
        retval = kevent(state->kqfd, NULL, 0, state->events, eventLoop->setsize,
                        NULL);
    }

    if (retval > 0) {
        int j;

        /*
         * 通常我们先执行读事件，然后执行写事件。
         * 当设置屏障时，我们会反过来执行。
         *
         * 然而，在 kqueue 下，读和写事件是分离的事件，
         * 这使得无法控制读写的顺序。因此我们存储获得的事件掩码，
         * 稍后合并相同 fd 的事件。
         */
        for (j = 0; j < retval; j++) {
            struct kevent *e = state->events+j;
            int fd = e->ident;
            int mask = 0;

            if (e->filter == EVFILT_READ) mask = AE_READABLE;
            else if (e->filter == EVFILT_WRITE) mask = AE_WRITABLE;
            add_event_mask(state->eventsMask, fd, mask);
        }

        /*
         * 重新遍历以合并读写事件，并将 fd 的掩码设为 0，
         * 以防再次遇到该 fd 时重复添加事件。
         */
        numevents = 0;
        for (j = 0; j < retval; j++) {
            struct kevent *e = state->events+j;
            int fd = e->ident;
            int mask = get_event_mask(state->eventsMask, fd);

            if (mask) {
                eventLoop->fired[numevents].fd = fd;
                eventLoop->fired[numevents].mask = mask;
                reset_event_mask(state->eventsMask, fd);
                numevents++;
            }
        }
    }
    return numevents;
}

/**
 * @brief 获取后端名称
 * @return 返回 "kqueue" 字符串
 */
static char *ae_api_name(void) {
    return "kqueue";
}

/**
 * @brief kqueue 后端的读操作（直接调用系统 read）
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
 * @brief kqueue 后端的写操作（直接调用系统 write）
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
 * @brief kqueue 后端的睡眠前钩子（空实现）
 * @param eventLoop 事件循环（未使用）
 */
static void ae_api_before_sleep(ae_event_loop_t *eventLoop) {
    AE_NOTUSED(eventLoop);
}

/**
 * @brief kqueue 后端的睡眠后钩子（空实现）
 * @param eventLoop 事件循环（未使用）
 */
static void ae_api_after_sleep(ae_event_loop_t *eventLoop) {
    AE_NOTUSED(eventLoop);
}