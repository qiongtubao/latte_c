/**
 * @file async_io_iouring.c
 * @brief 基于 Linux io_uring 的异步 I/O 实现
 *        每个线程维护独立的 io_uring 实例（通过 thread_single_object 管理），
 *        支持异步网络读写；完成事件通过 async_io_each_finished() 批量消费
 */

#include <liburing.h>
#include "async_io.h"
#include "thread_single_object/thread_single_object.h"
#include "log/log.h"
#include <errno.h>
#include "debug/latte_debug.h"
#include "utils/utils.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>

/** @brief thread_single_object 中 io_uring 实例的注册名称 */
#define SINGLE_IO_URING ("async_io_iouring")

/**
 * @brief 每个线程独有的 io_uring 运行时信息
 */
typedef struct io_uring_thread_info_t {
    struct io_uring io_uring_instance; /**< io_uring 实例（SQE/CQE 队列） */
    long long reqs_submitted;          /**< 累计已提交的请求数 */
    long long reqs_finished;           /**< 累计已完成的请求数 */
} io_uring_thread_info_t;

/**
 * @brief 初始化 io_uring 实例，队列深度为 1024
 * @param io_uring_instance 指向待初始化的 io_uring 结构
 * @return true 初始化成功；false 失败（已记录错误日志）
 */
bool init_io_uring(struct io_uring* io_uring_instance) {
    int ret = io_uring_queue_init(1024, io_uring_instance,  0);
    if (ret < 0) {
        LATTE_LIB_LOG(LOG_ERROR,"io_uring_queue_init failed: %s", strerror(-ret));
        return false;
    }
    return true;
}

/**
 * @brief 创建并初始化线程 io_uring 信息结构体
 * @return 新建的 io_uring_thread_info_t 指针
 */
io_uring_thread_info_t* io_uring_thread_info_new() {
    io_uring_thread_info_t* thread_info = (io_uring_thread_info_t*)zmalloc(sizeof(struct io_uring_thread_info_t));
    init_io_uring(&thread_info->io_uring_instance);
    thread_info->reqs_submitted = 0;
    thread_info->reqs_finished = 0;
    return thread_info;
}

/**
 * @brief 销毁线程 io_uring 信息结构体（thread_single_object 析构回调）
 *        退出 io_uring 队列并释放内存
 * @param arg io_uring_thread_info_t 指针（void* 类型以匹配回调签名）
 */
void io_uring_thread_info_delete(void* arg) {
    struct io_uring_thread_info_t* thread_info = (struct io_uring_thread_info_t*)arg;
    io_uring_queue_exit(&thread_info->io_uring_instance);
    zfree(thread_info);
}

/**
 * @brief 获取当前线程的 io_uring 信息，不存在时自动创建并注册
 * @return 当前线程的 io_uring_thread_info_t 指针
 */
struct io_uring_thread_info_t* get_or_new_thread_info() {
    struct io_uring_thread_info_t* thread_info = (struct io_uring_thread_info_t*)thread_single_object_get(SINGLE_IO_URING);
    if (thread_info == NULL) {
        thread_info = io_uring_thread_info_new();
        thread_single_object_set(SINGLE_IO_URING, thread_info);
    }
    return thread_info;
}


/**
 * @brief 提交异步网络写请求到 io_uring SQE 队列
 *        断言检查 request 各字段合法性后，通过 io_uring_prep_write 注册写操作
 * @param request 已创建的网络写请求对象
 * @return 提交成功返回 1
 */
int async_io_net_write(async_io_request_t* request) {
    latte_assert_with_info(request->fd >= 0, "fd is not valid\n");
    latte_assert_with_info(request->buf != NULL, "buf is not valid\n");
    latte_assert_with_info(request->len > 0, "len is not valid\n");
    latte_assert_with_info(request->callback != NULL, "callback is not valid\n");
    latte_assert_with_info(request->is_finished == false, "request is finished\n");
    latte_assert_with_info(request->is_canceled == false, "request is canceled\n");
    latte_assert_with_info(request->error == NULL, "error is not valid\n");
    latte_assert_with_info(request->type == ASYNC_IO_NET_WRITE, "type is not valid\n");
    struct io_uring_thread_info_t* thread_info = get_or_new_thread_info();
    struct io_uring* io_uring_instance = &thread_info->io_uring_instance;
    struct io_uring_sqe* sqe = io_uring_get_sqe(io_uring_instance);
    io_uring_prep_write(sqe, request->fd, request->buf, request->len, 0);
    /* 将 request 绑定到 SQE 的用户数据，CQE 完成时通过此指针找回请求 */
    io_uring_sqe_set_data(sqe, request);
    io_uring_submit(io_uring_instance);
    thread_info->reqs_submitted++;
    return 1;
}

/**
 * @brief 处理单个完成队列条目（CQE），根据请求类型设置错误信息并触发回调
 *        READ 类型：cqe->res < 0 时记录错误；WRITE 类型：当前仅 break（可扩展）
 * @param cqe 已完成的 io_uring CQE 指针
 */
void handle_cqe( struct io_uring_cqe* cqe) {
    async_io_request_t *req = (async_io_request_t *)io_uring_cqe_get_data(cqe);
    latte_assert_with_info(req->is_finished == false, "request is finished");
    switch (req->type)
        {
        case ASYNC_IO_NET_WRITE:
            break;
        case ASYNC_IO_FILE_WRITE:
            break;
        case ASYNC_IO_NET_READ:
            if (cqe->res < 0) {
                LATTE_LIB_LOG(LOG_ERROR, "Async read socket(%d) failed: %d %s", req->fd, (cqe->res), strerror(-(cqe->res)));
                req->error = error_new(CIOError, "async_io_net_read", "%d %s",(cqe->res), strerror(-(cqe->res)));
            }
        break;
        case ASYNC_IO_FILE_READ:
            if (cqe->res < 0) {
                LATTE_LIB_LOG(LOG_ERROR, "Async read file(%d) failed: %d %s ", req->fd, (cqe->res), strerror(-(cqe->res)));
                req->error = error_new(CIOError, "async_io_net_read", "%d %s", (cqe->res), strerror(-(cqe->res)));
            }
            break;
    default:
        break;
    }
    /* 标记完成并触发用户回调 */
    latte_assert_with_info(req->callback != NULL, "callback is NULL");
    req->is_finished = true;
    req->callback(req);

}


/**
 * @brief 消费当前线程 io_uring CQE 队列中所有已完成的请求
 *        若 submitted == finished 则跳过（无待处理事件）；
 *        批量调用 handle_cqe 后通过 io_uring_cq_advance 一次性推进消费指针
 * @return 本次消费的完成事件数量；无事件或未提交返回 0
 */
int async_io_each_finished() {
    struct io_uring_thread_info_t* thread_info = get_or_new_thread_info();
    if (thread_info->reqs_submitted == thread_info->reqs_finished) {
        return 0;
    }
    struct io_uring* io_uring_instance = &thread_info->io_uring_instance;

    struct io_uring_cqe* cqe;
    unsigned head;
    unsigned count = 0;

    /* 遍历 CQE 环，处理每个完成事件 */
    io_uring_for_each_cqe(io_uring_instance, head, cqe) {
        handle_cqe(cqe);
        count++;

    }
    /* 批量推进 CQE 消费指针，避免逐个调用 io_uring_cqe_seen */
    io_uring_cq_advance(io_uring_instance, count);
    thread_info->reqs_finished += count;
    return count;
}

/** @brief 模块是否已初始化的标志 */
static bool is_init = false;

/**
 * @brief 全局初始化 async_io 模块
 *        注册 io_uring 线程本地对象类型（幂等，重复调用安全）
 * @return 始终返回 1
 */
int async_io_module_init() {
    if (is_init) {
        return 1;
    }
    thread_single_object_module_init();
    thread_single_object_register(SINGLE_IO_URING, io_uring_thread_info_delete);
    is_init = true;
    return 1;
}

/**
 * @brief 全局销毁 async_io 模块
 *        删除当前线程 io_uring 实例并注销类型（幂等，重复调用安全）
 * @return 始终返回 1
 */
int async_io_module_destroy() {
    if (!is_init) {
        return 1;
    }
    thread_single_object_delete(SINGLE_IO_URING);
    thread_single_object_unregister(SINGLE_IO_URING);
    is_init = false;
    return 1;
}

/**
 * @brief 线程级初始化（当前为空实现，预留扩展）
 * @return 始终返回 1
 */
int async_io_module_thread_init() {
    return 1;
}

/**
 * @brief 线程级销毁：删除当前线程的 io_uring 实例
 * @return 始终返回 1
 */
int async_io_module_thread_destroy() {
    thread_single_object_delete(SINGLE_IO_URING);
    return 1;
}
