
#ifndef __LATTE_BENCHMARK_H
#define __LATTE_BENCHMARK_H
#include <stdio.h>
#include <stdlib.h>
#include "sds/sds.h"
#include "ae/ae.h"
#include "utils/atomic.h"
#include "list/list.h"

/**
 * @brief 向连接写入数据的函数类型
 * @param context 连接上下文指针
 * @param ptr     待写入数据指针
 * @param len     数据长度（字节）
 * @return int*   写入结果（实现定义）
 */
typedef int *(*writeConnFunc)(void *context, void* ptr, ssize_t len);

/**
 * @brief 从连接读取数据的函数类型
 * @param context 连接上下文指针
 * @return ssize_t* 读取结果（实现定义）
 */
typedef ssize_t *(*readConnFunc)(void *context);

/**
 * @brief 基准测试连接上下文
 * 封装网络连接的文件描述符、错误信息及读写函数指针。
 */
typedef struct benchmarkContext {
    int err;            /**< 错误标志，0 表示无错误 */
    char errstr[128];   /**< 错误描述字符串 */
    int fd;             /**< 连接文件描述符 */
    writeConnFunc write; /**< 写数据函数指针 */
    readConnFunc read;   /**< 读数据函数指针 */
} benchmarkContext;

/**
 * @brief 向客户端写入数据的函数类型
 * @param client 客户端指针
 * @param ptr    待写入数据
 * @param len    数据长度
 * @return ssize_t 实际写入字节数
 */
typedef ssize_t (*writeContextFunc)(void *client, void* ptr, ssize_t len);

/**
 * @brief 读取响应的事件处理函数类型
 * @param client 客户端指针
 * @return ssize_t 读取字节数
 */
typedef ssize_t (*readHandlerFunc)(void *client);

/**
 * @brief 构造写入内容的函数类型（生成待发送命令）
 * @param client 客户端指针
 * @return ssize_t 内容长度
 */
typedef ssize_t (*createWriteContentFunc)(void *client);

/**
 * @brief 基准测试客户端结构体（typedef 为指针类型）
 * 每个客户端对应一条连接，负责发送命令并记录延迟。
 */
typedef struct _client {
    benchmarkContext *context;              /**< 底层连接上下文 */
    sds_t obuf;                             /**< 发送缓冲区（SDS 字符串） */
    int thread_id;                          /**< 所属线程索引，-1 表示主线程 */
    int slots_last_update;                  /**< 槽位最后更新的时间戳快照 */
    int written;                            /**< 当前请求已写入字节数 */
    long long start;                        /**< 请求开始时间（微秒） */
    long long latency;                      /**< 请求延迟（微秒），-1 表示未完成 */
    createWriteContentFunc createWriteContent; /**< 构造发送内容的回调 */
    writeContextFunc writeContext;          /**< 向连接写入数据的回调 */
    readHandlerFunc readHandler;            /**< 读取响应的回调 */
} *client;

/**
 * @brief 基准测试线程结构体
 * 每个线程拥有独立的事件循环，并发执行测试请求。
 */
typedef struct benchmarkThread {
    int index;          /**< 线程序号（从 0 开始） */
    pthread_t thread;   /**< POSIX 线程句柄 */
    aeEventLoop *el;    /**< 该线程独立的 ae 事件循环 */
} benchmarkThread;

/**
 * @brief 基准测试全局控制结构体
 * 管理所有测试线程、客户端列表及连接配置。
 */
typedef struct benchmark {
    int num_threads;                    /**< 测试线程数量 */
    aeEventLoop *el;                    /**< 主线程事件循环（单线程模式使用） */
    const char *hostip;                 /**< 目标服务器 IP 地址 */
    int hostport;                       /**< 目标服务器端口 */
    struct benchmarkThread **threads;   /**< 测试线程数组 */
    list_t *clients;                    /**< 所有活跃客户端链表 */
    latteAtomic int liveclients;        /**< 当前存活客户端数量（原子变量） */
    latteAtomic int slots_last_update;  /**< 插槽最后更新时间（原子变量） */
} benchmark;

/**
 * @brief 初始化基准测试结构体
 * 创建客户端列表和事件循环，将线程数组置 NULL。
 * @param be 目标 benchmark 结构体指针
 * @return int 成功返回 1
 */
int initBenchmark(struct benchmark* be);

/**
 * @brief 启动基准测试
 * 创建客户端连接，按线程数选择单线程或多线程事件循环。
 * @param be    目标 benchmark 结构体指针
 * @param title 测试名称（用于展示）
 * @param cmd   待发送命令内容
 * @param len   命令长度（字节）
 * @return int 成功返回 1
 */
int startBenchmark(struct benchmark* be, char *title, char *cmd, int len);


#endif
