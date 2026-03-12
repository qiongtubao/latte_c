/**
 * @file async_io_dispatch.c
 * @brief macOS Grand Central Dispatch 异步I/O实现
 *        使用macOS的GCD框架实现异步I/O操作
 *        通过串行队列保证线程安全，适用于macOS/iOS平台
 */

#include "async_io.h"
#include <dispatch/dispatch.h>
#include "error/error.h"
#include "thread_single_object/thread_single_object.h"
#define SINGLE_IO_DISPATCH ("async_io_dispatch")
#include "list/list.h"
#include "log/log.h"

/**
 * @brief GCD线程信息结构
 *        存储每个线程的调度队列和运行状态
 */
typedef struct dispatch_thread_info_t {
    dispatch_queue_t queue;         /**< GCD串行队列 */
    list_t* running_requests;       /**< 正在运行的请求列表 */
    long long reqs_submitted;       /**< 已提交的请求数量 */
    long long reqs_finished;        /**< 已完成的请求数量 */
} dispatch_thread_info_t;

/**
 * @brief 创建新的调度线程信息
 *        初始化GCD队列和请求列表
 * @return 新创建的线程信息结构
 */
dispatch_thread_info_t* dispatch_thread_info_new() {
    dispatch_thread_info_t* thread_info = (dispatch_thread_info_t*)zmalloc(sizeof(struct dispatch_thread_info_t));
    // 创建串行队列，保证线程安全
    thread_info->queue = dispatch_queue_create(SINGLE_IO_DISPATCH, DISPATCH_QUEUE_SERIAL);
    thread_info->reqs_submitted = 0;
    thread_info->reqs_finished = 0;
    thread_info->running_requests = list_new();  // 初始化请求列表
    return thread_info;
}

/**
 * @brief 获取或创建当前线程的调度信息
 *        使用线程单例模式，每个线程一个调度器实例
 * @return 当前线程的调度线程信息
 */
struct dispatch_thread_info_t* get_or_new_thread_info() {
    struct dispatch_thread_info_t* thread_info = (struct dispatch_thread_info_t*)thread_single_object_get(SINGLE_IO_DISPATCH);
    if (thread_info == NULL) {
        thread_info = dispatch_thread_info_new();
        thread_single_object_set(SINGLE_IO_DISPATCH, thread_info);
    }
    return thread_info;
}

/**
 * @brief 检查请求是否需要删除
 *        判断请求是否已完成或出错，如需删除则调用回调
 * @param value 请求对象（void*类型）
 * @return 1 需要删除；0 继续保留
 */
int request_need_delete(void* value) {
    async_io_request_t* request = (async_io_request_t*)value;
    int need_delete = request->is_finished || request->error != NULL;
    if (need_delete) {
        request->callback(request);  // 调用完成回调
    }
    return need_delete;
}

/**
 * @brief 处理每个已完成的请求
 *        遍历运行中的请求列表，删除已完成的请求
 * @return 已处理的请求数量
 */
int async_io_each_finished() {
    struct dispatch_thread_info_t* thread_info = get_or_new_thread_info();
    list_t* running_requests = thread_info->running_requests;
    return list_for_each_delete(running_requests, request_need_delete);
}

/**
 * @brief 执行异步网络写入操作
 *        将写入操作提交到GCD队列异步执行
 * @param request 网络写入请求
 * @return 0 成功提交
 */
int async_io_net_write(async_io_request_t* request)  {
    struct dispatch_thread_info_t* thread_info = get_or_new_thread_info();
    dispatch_queue_t queue = thread_info->queue;
    // 将请求添加到运行列表
    list_add_node_tail(thread_info->running_requests, request);

    // 提交异步任务到GCD队列
    dispatch_async(queue, ^{
        ssize_t result = write(request->fd, request->buf, request->len);
        int success = result > 0;
        if (!success) {
            // 写入失败，创建错误对象
            request->error = error_new(CIOError, "dispatch", "write failed");
        } else {
            // 写入成功，标记为已完成
            request->is_finished = true;
        }
    });

    return 0;
}

/**
 * @brief 删除调度线程信息
 *        清理GCD队列和相关资源
 * @param arg 线程信息结构（void*类型）
 */
void dispatch_thread_info_delete(void* arg) {
    struct dispatch_thread_info_t* thread_info = (struct dispatch_thread_info_t*)arg;
    dispatch_release(thread_info->queue);  // 释放GCD队列
    list_delete(thread_info->running_requests);  // 删除请求列表
    zfree(thread_info);
}

static bool is_init = false;

/**
 * @brief 初始化异步I/O模块
 *        注册线程单例对象和清理函数
 * @return 1 成功
 */
int async_io_module_init() {
    if (is_init) {
        return 1;
    }
    thread_single_object_module_init();
    // 注册线程单例对象和清理函数
    thread_single_object_register(SINGLE_IO_DISPATCH, dispatch_thread_info_delete);
    is_init = true;
    return 1;
}

/**
 * @brief 销毁异步I/O模块
 *        清理所有线程单例对象
 * @return 1 成功
 */
int async_io_module_destroy() {
    if (!is_init) {
        return 1;
    }
    thread_single_object_delete(SINGLE_IO_DISPATCH);
    thread_single_object_unregister(SINGLE_IO_DISPATCH);
    is_init = false;
    return 1;
}

/**
 * @brief 初始化线程的异步I/O模块
 *        当前实现为空，预留接口
 * @return 1 成功
 */
int async_io_module_thread_init() {
    return 1;
}

/**
 * @brief 销毁线程的异步I/O模块
 *        删除当前线程的调度器实例
 * @return 1 成功
 */
int async_io_module_thread_destroy() {
    thread_single_object_delete(SINGLE_IO_DISPATCH);
    return 1;
}