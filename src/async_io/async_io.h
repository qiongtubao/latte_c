/**
 * @file async_io.h
 * @brief 异步 I/O 模块接口
 *        基于 io_uring（Linux）或 dispatch（macOS）实现非阻塞网络/文件读写
 */

#ifndef __LATTE_ASYNC_IO_H
#define __LATTE_ASYNC_IO_H

#include <stdbool.h>
#include "error/error.h"

/**
 * @brief 异步 I/O 请求类型
 */
typedef enum async_io_type {
    ASYNC_IO_NET_READ,    /**< 网络读 */
    ASYNC_IO_NET_WRITE,   /**< 网络写 */
    ASYNC_IO_FILE_READ,   /**< 文件读 */
    ASYNC_IO_FILE_WRITE,  /**< 文件写 */
} async_io_type;


/**
 * @brief 异步 I/O 请求描述结构体
 *        每次 I/O 操作对应一个请求对象，完成后通过 callback 通知调用方
 */
typedef struct async_io_request_t {
    async_io_type type;       /**< 请求类型：网络/文件 × 读/写 */
    int fd;                   /**< 操作的文件描述符 */
    char* buf;                /**< 数据缓冲区指针 */
    int len;                  /**< 缓冲区长度（字节） */
    size_t offset;            /**< 文件操作的起始偏移量（网络操作为 0） */
    void* ctx;                /**< 用户自定义上下文，回调中透传 */
    void (*callback)(struct async_io_request_t* request); /**< 完成回调函数 */
    bool is_finished;         /**< 请求是否已完成 */
    bool is_canceled;         /**< 请求是否已取消 */
    latte_error_t* error;     /**< 错误信息，成功时为 NULL */
} async_io_request_t;

/**
 * @brief 创建一个网络写请求对象
 * @param fd       目标文件描述符
 * @param buf      待写入数据缓冲区
 * @param len      写入字节数
 * @param ctx      用户上下文
 * @param callback 完成回调
 * @return 新建的请求对象指针
 */
async_io_request_t* net_write_request_new(int fd, char* buf, size_t len, void* ctx, void (*callback)( async_io_request_t* request));

/**
 * @brief 创建一个网络读请求对象
 * @param fd       目标文件描述符
 * @param buf      接收数据缓冲区
 * @param len      最大读取字节数
 * @param ctx      用户上下文
 * @param callback 完成回调
 * @return 新建的请求对象指针
 */
async_io_request_t* net_read_request_new(int fd, char* buf, size_t len, void* ctx, void (*callback)( async_io_request_t* request));

/**
 * @brief 销毁异步 I/O 请求对象，释放内部错误信息及结构体内存
 * @param request 要释放的请求对象
 */
void async_io_request_delete(async_io_request_t* request);

/**
 * @brief 全局初始化 async_io 模块（注册 io_uring / dispatch 资源）
 * @return 成功返回 1
 */
int async_io_module_init();

/**
 * @brief 全局销毁 async_io 模块，释放全局资源
 * @return 成功返回 1
 */
int async_io_module_destroy();

/**
 * @brief 线程级初始化（可扩展，当前为空实现）
 * @return 成功返回 1
 */
int async_io_module_thread_init();

/**
 * @brief 线程级销毁，释放当前线程的 io_uring 实例
 * @return 成功返回 1
 */
int async_io_module_thread_destroy();

/**
 * @brief 提交一个异步网络读请求
 * @param request 已创建的网络读请求对象
 * @return 提交成功返回 1
 */
int async_io_net_read(async_io_request_t* request);

/**
 * @brief 提交一个异步网络写请求
 * @param request 已创建的网络写请求对象
 * @return 提交成功返回 1
 */
int async_io_net_write(async_io_request_t* request);

/**
 * @brief 提交一个异步文件读请求
 * @param request 已创建的文件读请求对象
 * @return 提交成功返回 1
 */
int async_io_file_read(async_io_request_t* request);

/**
 * @brief 提交一个异步文件写请求
 * @param request 已创建的文件写请求对象
 * @return 提交成功返回 1
 */
int async_io_file_write(async_io_request_t* request);

/**
 * @brief 处理当前线程所有已完成的 I/O 请求（消费 CQE），触发各自回调
 * @return 本次处理的完成事件数量
 */
int async_io_each_finished();


#endif
