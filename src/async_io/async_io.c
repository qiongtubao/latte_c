/**
 * @file async_io.c
 * @brief 异步I/O操作的跨平台抽象层
 *        根据编译时配置选择不同的异步I/O后端：
 *        - Linux: io_uring (HAVE_LIBURING)
 *        - macOS: Grand Central Dispatch (HAVE_DISPATCH)
 */

#include "utils/sys_config.h"
#ifdef HAVE_LIBURING
    #include "async_io_iouring.c"
#elif HAVE_DISPATCH
    #include "async_io_dispatch.c"
#endif

#include "async_io.h"
#include "zmalloc/zmalloc.h"

/**
 * @brief 创建新的异步I/O请求
 * @param fd       文件描述符
 * @param buf      数据缓冲区
 * @param len      数据长度
 * @param ctx      用户上下文数据
 * @param callback 完成回调函数
 * @return 新创建的请求对象；失败返回NULL
 */
async_io_request_t* async_io_request_new(int fd, char* buf, size_t len, void* ctx, void (*callback)(async_io_request_t* request)) {
    async_io_request_t* request = (async_io_request_t*)zmalloc(sizeof(async_io_request_t));
    request->fd = fd;                    // 设置文件描述符
    request->buf = buf;                  // 设置数据缓冲区
    request->len = len;                  // 设置数据长度
    request->offset = 0;                 // 初始化偏移量
    request->ctx = ctx;                  // 设置用户上下文
    request->callback = callback;        // 设置完成回调
    request->is_finished = false;        // 标记为未完成
    request->is_canceled = false;        // 标记为未取消
    request->error = NULL;               // 初始化错误为空
    return request;
}

/**
 * @brief 创建网络写入请求
 *        特化的异步写入请求，用于网络套接字
 * @param fd       网络文件描述符
 * @param buf      要写入的数据
 * @param len      数据长度
 * @param ctx      用户上下文
 * @param callback 完成回调函数
 * @return 新创建的网络写入请求
 */
async_io_request_t* net_write_request_new(int fd, char* buf, size_t len, void* ctx, void (*callback)( async_io_request_t* request)) {
    async_io_request_t* request = async_io_request_new(fd, buf, len, ctx, callback);
    request->type = ASYNC_IO_NET_WRITE;  // 设置请求类型为网络写入
    return request;
}

/**
 * @brief 创建网络读取请求
 *        特化的异步读取请求，用于网络套接字
 * @param fd       网络文件描述符
 * @param buf      读取缓冲区
 * @param len      缓冲区大小
 * @param ctx      用户上下文
 * @param callback 完成回调函数
 * @return 新创建的网络读取请求
 */
async_io_request_t* net_read_request_new(int fd, char* buf, size_t len, void* ctx, void (*callback)( async_io_request_t* request)) {
    async_io_request_t* request = async_io_request_new(fd, buf, len, ctx, callback);
    request->type = ASYNC_IO_NET_READ;   // 设置请求类型为网络读取
    return request;
}

/**
 * @brief 删除异步I/O请求
 *        释放请求相关的所有资源，包括错误信息
 * @param request 要删除的请求对象
 */
void async_io_request_delete(async_io_request_t* request) {
    // 如果存在错误信息且不是OK状态，则释放错误对象
    if (request->error != NULL && !error_is_ok(request->error)) {
        error_delete(request->error);
        request->error = NULL;
    }
    zfree(request);  // 释放请求结构体内存
}