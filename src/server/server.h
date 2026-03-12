

#ifndef __LATTE_SERVER_H
#define __LATTE_SERVER_H

#include "ae/ae.h"
#include "connection/connection.h"
#include "sds/sds.h"
#include "dict/dict.h"
#include "anet/anet.h"
#include <pthread.h>
#include "sds/sds.h"
#include "config/config.h"
#include "list/list.h"
#include "endianconv/endianconv.h"
#include "rax/rax.h"
#include <stdio.h>
#include "utils/atomic.h"
#include "log/log.h"
#include "cron.h"
#include "async_io/async_io.h"

/* ==================== 错误码 ==================== */
#define SERVER_OK                    0   /**< 操作成功 */
#define SERVER_ERR                   -1  /**< 操作失败 */

/** @brief 最大绑定地址数量 */
#define CONFIG_BINDADDR_MAX 16

/** @brief anet 错误信息缓冲区长度 */
#define ANET_ERR_LEN 256

/* ==================== Socket 文件描述符集合 ==================== */
/** @brief 客户端结构体前向声明 */
struct latte_client_t;

/**
 * @brief 监听套接字文件描述符集合
 * 用于管理服务器绑定的多个监听 socket。
 */
typedef struct socket_fds_t {
    int fd[CONFIG_BINDADDR_MAX];    /**< 监听 socket fd 数组，最多 CONFIG_BINDADDR_MAX 个 */
    int count;                      /**< 当前有效 fd 数量 */
} socket_fds_t;

/* ==================== 函数指针类型定义 ==================== */
/**
 * @brief TCP 连接接受处理函数类型
 * @param el       事件循环指针
 * @param fd       触发事件的文件描述符
 * @param privdata 用户数据（通常为 server 指针）
 * @param mask     事件掩码
 */
typedef void (*tcp_handler_func)(ae_event_loop_t *el, int fd, void *privdata, int mask);

/**
 * @brief 创建客户端函数类型，返回新分配的 latte_client_t 指针
 */
typedef struct latte_client_t *(*create_client_func)();

/**
 * @brief 释放客户端函数类型
 * @param client 要释放的客户端指针
 */
typedef void (*free_client_func)(struct latte_client_t* client);

/**
 * @brief latte 服务器核心结构体
 *
 * 包含事件循环、网络监听、客户端管理、定时任务管理等所有服务器状态。
 */
typedef struct latte_server_t {
    /* ===== 基本信息 ===== */
    pid_t pid;                          /**< 服务器进程 ID */
    pthread_t main_thread_id;           /**< 主线程 ID */
    ae_event_loop_t *el;                /**< 事件循环指针 */

    /* ===== 网络配置 ===== */
    long long port;                     /**< 监听端口号 */
    socket_fds_t ipfd;                  /**< TCP 监听 socket 集合 */
    vector_t* bind;                     /**< 绑定地址列表（SDS 字符串向量） */
    char neterr[ANET_ERR_LEN];          /**< anet 网络错误信息缓冲区 */
    long long tcp_backlog;              /**< TCP listen() 积压队列长度 */

    /* ===== 客户端管理 ===== */
    tcp_handler_func acceptTcpHandler;  /**< TCP 连接接受处理函数（事件回调） */
    create_client_func createClient;    /**< 客户端创建工厂函数 */
    free_client_func freeClient;        /**< 客户端释放函数 */
    latteAtomic uint64_t next_client_id;/**< 原子递增的客户端 ID 计数器 */
    list_t *clients;                    /**< 所有活跃客户端链表 */
    unsigned int maxclients;            /**< 最大并发客户端数量 */
    rax *clients_index;                 /**< 以客户端 ID 为键的基数树索引 */
    list_t* clients_pending_write;      /**< 待写入数据的客户端队列（同步 IO） */
    list_t* clients_async_pending_write;/**< 待写入数据的客户端队列（异步 IO） */

    /* ===== 定时任务 ===== */
    cron_manager_t* cron_manager;       /**< 定时任务管理器 */

    /* ===== 异步 IO ===== */
    bool use_async_io;                  /**< 是否启用异步 IO 模式 */
} latte_server_t;


/* ==================== 服务器生命周期接口 ==================== */

/**
 * @brief 启动 latte 服务器（监听端口、注册事件、进入事件循环）
 * @param server 已初始化的服务器指针
 * @return int 成功返回 1，失败调用 exit(1)
 */
int start_latte_server(struct latte_server_t* server);

/**
 * @brief 停止 latte 服务器（执行 before-sleep 任务并停止事件循环）
 * @param server 目标服务器指针
 * @return int 始终返回 1
 */
int stop_latte_server(struct latte_server_t* server);

/**
 * @brief 初始化 latte 服务器的内部状态（客户端列表、索引、定时器等）
 * @param server 目标服务器指针
 */
void init_latte_server(struct latte_server_t* server);

/**
 * @brief 销毁 latte 服务器，释放所有内部资源
 * @param server 目标服务器指针
 */
void destory_latte_server(struct latte_server_t* server);



/* latteClient */

// typedef int *(*exec_handler)(struct latte_client_t* client);
// typedef struct latte_client_t {
//     uint64_t id;
//     connection *conn;
//     sds_t querybuf;    /* 我们用来累积客户端查询的缓冲区 */
//     size_t qb_pos;
//     size_t querybuf_peak;   /* 最近（100毫秒或更长时间）查询缓冲区大小的峰值 */
//     exec_handler exec;
//     int flags;
//     struct latteServer* server;
//     struct list_node_t* client_list_node;
// } latte_client_t;

// void initInnerLatteClient(struct latteClient* client);
// void freeInnerLatteClient(struct latteClient* client);

void read_query_from_client(connection *conn);
#endif