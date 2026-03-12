/**
 * @file ae_select.c
 * @brief POSIX select() 多路复用后端实现
 *        select() 是最原始的多路复用机制，兼容性最好但性能相对较差
 *        适用于连接数较少的场景或作为兜底方案
 */

#include "ae.h"
#include <sys/select.h>
#include <string.h>

/**
 * @brief select 后端状态结构
 */
typedef struct ae_api_state_t {
    fd_set rfds, wfds;       /**< 读/写文件描述符集合 */
    /* We need to have a copy of the fd sets as it's not safe to reuse
     * FD sets after select(). */
    fd_set _rfds, _wfds;     /**< fd集合副本，用于select调用 */
} ae_api_state_t;

/**
 * @brief 创建 select 后端
 *        初始化文件描述符集合
 * @param eventLoop 目标事件循环
 * @return 0 成功；-1 失败
 */
static int ae_api_create(ae_event_loop_t *eventLoop) {
    LATTE_LIB_LOG(LOG_DEBUG, "[ae_api_create] ae use select");
    ae_api_state_t *state = zmalloc(sizeof(ae_api_state_t));

    if (!state) return -1;
    FD_ZERO(&state->rfds);    // 清空读fd集合
    FD_ZERO(&state->wfds);    // 清空写fd集合
    eventLoop->apidata = state;
    return 0;
}

/**
 * @brief 调整后端大小
 *        检查新大小是否超出select限制
 * @param eventLoop 事件循环实例
 * @param setsize   新的文件描述符集合大小
 * @return 0 成功；-1 失败
 */
static int ae_api_resize(ae_event_loop_t *eventLoop, int setsize) {
    /* Just ensure we have enough room in the fd_set type. */
    if (setsize >= FD_SETSIZE) return -1;  // 超出select限制
    return 0;
}

/**
 * @brief 删除 select 后端
 *        释放状态结构内存
 * @param eventLoop 事件循环实例
 */
static void ae_api_delete(ae_event_loop_t *eventLoop) {
    zfree(eventLoop->apidata);
}

/**
 * @brief 添加文件描述符事件到select集合
 * @param eventLoop 事件循环实例
 * @param fd        文件描述符
 * @param mask      事件掩码（AE_READABLE/AE_WRITABLE）
 * @return 0 成功
 */
static int ae_api_add_event(ae_event_loop_t *eventLoop, int fd, int mask) {
    ae_api_state_t *state = eventLoop->apidata;

    if (mask & AE_READABLE) FD_SET(fd,&state->rfds);  // 添加到读集合
    if (mask & AE_WRITABLE) FD_SET(fd,&state->wfds);  // 添加到写集合
    return 0;
}

/**
 * @brief 从select集合删除文件描述符事件
 * @param eventLoop 事件循环实例
 * @param fd        文件描述符
 * @param delmask   要删除的事件掩码
 */
static void ae_api_del_event(ae_event_loop_t *eventLoop, int fd, int delmask) {
    ae_api_state_t *state = eventLoop->apidata;

    if (delmask & AE_READABLE) FD_CLR(fd,&state->rfds);  // 从读集合移除
    if (delmask & AE_WRITABLE) FD_CLR(fd,&state->wfds);  // 从写集合移除
}

/**
 * @brief select 轮询函数
 *        等待文件描述符就绪事件
 * @param eventLoop 事件循环实例
 * @param tvp       超时时间
 * @return 就绪事件数量
 */
static int ae_api_poll(ae_event_loop_t *eventLoop, struct timeval *tvp) {
    ae_api_state_t *state = eventLoop->apidata;
    int retval, j, numevents = 0;

    // 复制fd集合，因为select会修改集合内容
    memcpy(&state->_rfds,&state->rfds,sizeof(fd_set));
    memcpy(&state->_wfds,&state->wfds,sizeof(fd_set));

    // 调用select等待事件就绪
    retval = select(eventLoop->maxfd+1,
                &state->_rfds,&state->_wfds,NULL,tvp);
    if (retval > 0) {
        // 遍历所有可能的fd，检查哪些已就绪
        for (j = 0; j <= eventLoop->maxfd; j++) {
            int mask = 0;
            ae_file_event_t *fe = &eventLoop->events[j];

            if (fe->mask == AE_NONE) continue;
            // 检查读事件就绪
            if (fe->mask & AE_READABLE && FD_ISSET(j,&state->_rfds))
                mask |= AE_READABLE;
            // 检查写事件就绪
            if (fe->mask & AE_WRITABLE && FD_ISSET(j,&state->_wfds))
                mask |= AE_WRITABLE;
            eventLoop->fired[numevents].fd = j;
            eventLoop->fired[numevents].mask = mask;
            numevents++;
        }
    }
    return numevents;
}

/**
 * @brief 获取后端名称
 * @return 后端名称字符串
 */
static char *ae_api_name(void) {
    return "select";
}

/**
 * @brief 读取数据（直接调用系统read）
 * @param eventLoop 事件循环（未使用）
 * @param fd        文件描述符
 * @param buf       缓冲区
 * @param buf_len   缓冲区大小
 * @return 读取的字节数；-1表示错误
 */
static int ae_api_read(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len) {
    AE_NOTUSED(eventLoop);
    return read(fd, buf, buf_len);
}

/**
 * @brief 写入数据（直接调用系统write）
 * @param eventLoop 事件循环（未使用）
 * @param fd        文件描述符
 * @param buf       数据缓冲区
 * @param buf_len   数据长度
 * @return 写入的字节数；-1表示错误
 */
static int ae_api_write(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len) {
    AE_NOTUSED(eventLoop);
    return write(fd, buf, buf_len);
}

/**
 * @brief 睡眠前钩子函数（空实现）
 * @param eventLoop 事件循环（未使用）
 */
static void ae_api_before_sleep(ae_event_loop_t *eventLoop) {
    AE_NOTUSED(eventLoop);
}

/**
 * @brief 睡眠后钩子函数（空实现）
 * @param eventLoop 事件循环（未使用）
 */
static void ae_api_after_sleep(ae_event_loop_t *eventLoop) {
    AE_NOTUSED(eventLoop);
}