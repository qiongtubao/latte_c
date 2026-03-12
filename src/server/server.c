#include "server.h"
#include <errno.h>
#include <string.h>
#include "utils/atomic.h"
#include "client.h"

/**
 * @brief 原子获取并递增下一个客户端 ID
 * @param server 服务器指针
 * @return uint64_t 分配到的唯一客户端 ID
 */
uint64_t getClientId(struct latte_server_t* server) {
    uint64_t client_id;
    latte_atomic_get_incr(server->next_client_id, client_id, 1);
    return client_id;
}

/**
 * @brief 关闭 socket_fds_t 中所有监听套接字并从事件循环移除可读事件
 * @param server 服务器指针（持有事件循环）
 * @param sfd    要关闭的 socket 集合指针，count 置 0
 */
void closeSocketListeners(struct latte_server_t* server, socket_fds_t *sfd) {
    int j;

    for (j = 0; j < sfd->count; j++) {
        if (sfd->fd[j] == -1) continue;

        ae_file_event_delete(server->el, sfd->fd[j], AE_READABLE);
        close(sfd->fd[j]);
    }

    sfd->count = 0;
}

/**
 * @brief 遍历绑定地址列表，创建 TCP 监听 socket 并填充 sfd
 *
 * 对每个绑定地址（支持 IPv4/IPv6），尝试创建 TCP 服务端 socket，
 * 设置非阻塞（anetNonBlock）和 close-on-exec（anetCloexec）。
 * 地址以 '-' 开头表示可选，监听失败不视为致命错误。
 *
 * @param server     服务器指针（用于日志和关闭监听器）
 * @param neterr     错误信息输出缓冲区
 * @param bind       绑定地址向量（SDS 字符串列表）
 * @param port       监听端口号
 * @param tcp_backlog TCP 积压队列长度
 * @param sfd        输出：填充成功的监听 socket 集合
 * @return int 至少绑定一个地址返回 1，所有地址均失败返回 -1
 */
int listenToPort(struct latte_server_t* server,char* neterr, vector_t* bind, int port, int tcp_backlog, socket_fds_t *sfd) {
    int j;
    latte_iterator_t* iterator = vector_get_iterator(bind);
    while(latte_iterator_has_next(iterator)) {
        sds addr = latte_iterator_next(iterator);
        int optional = *addr == '-';
        if (optional) addr++;
        if (strchr(addr, ':')) {
            sfd->fd[sfd->count] = anetTcp6Server(neterr,port,addr,tcp_backlog);
        } else {
            sfd->fd[sfd->count] = anetTcpServer(neterr ,port,"*",tcp_backlog);
        }
        if (sfd->fd[sfd->count] == ANET_ERR) {
            int net_errno = errno;
            LATTE_LIB_LOG(LOG_ERROR,
                "Warning: Could not create_server TCP listening socket %s:%d: %s\n",
                addr, port, neterr);
            if (net_errno == EADDRNOTAVAIL && optional) {
                continue;
            } 
            if (net_errno == ENOPROTOOPT     || net_errno == EPROTONOSUPPORT ||
                net_errno == ESOCKTNOSUPPORT || net_errno == EPFNOSUPPORT ||
                net_errno == EAFNOSUPPORT)
                continue;
            
            closeSocketListeners(server, sfd);
            return -1;
        }
        anetNonBlock(NULL, sfd->fd[sfd->count]);
        anetCloexec(sfd->fd[sfd->count]);
        sfd->count++;
    }
    latte_iterator_delete(iterator);

    // char** bindaddr;
    // int bindaddr_count;
    // char *default_bindaddr[2] = {"*", "-::*"};
    // if (bind == NULL 
    //     || (bind != NULL && vector_size(bind) == 0)) {
    //     bindaddr = default_bindaddr;
    //     bindaddr_count = 2;
    // } else {
    //     bindaddr = (char**)bind->data;
    //     bindaddr_count = vector_size(bind);
    // }
    // for (j = 0; j < bindaddr_count; j++) {
    //     char* addr = bindaddr[j];
    //     int optional = *addr == '-';
    //     if (optional) addr++;
    //     if (strchr(addr, ':')) {
    //         sfd->fd[sfd->count] = anetTcp6Server(neterr,port,addr,tcp_backlog);
    //     } else {
    //         sfd->fd[sfd->count] = anetTcpServer(neterr,port,addr,tcp_backlog);
    //     }

    //     if (sfd->fd[sfd->count] == ANET_ERR) {
    //         int net_errno = errno;
    //         log_error("latte_c",
    //             "Warning: Could not create_server TCP listening socket %s:%d: %s\n",
    //             addr, port, neterr);
    //         if (net_errno == EADDRNOTAVAIL && optional) {
    //             continue;
    //         } 
    //         if (net_errno == ENOPROTOOPT     || net_errno == EPROTONOSUPPORT ||
    //             net_errno == ESOCKTNOSUPPORT || net_errno == EPFNOSUPPORT ||
    //             net_errno == EAFNOSUPPORT)
    //             continue;
            
    //         closeSocketListeners(server, sfd);
    //         return -1;
    //     }
    //     anetNonBlock(NULL, sfd->fd[sfd->count]);
    //     anetCloexec(sfd->fd[sfd->count]);
    //     sfd->count++;
    // }
    return 1;
}

/**
 * @brief 为所有监听 socket 批量注册可读事件（接受新连接）
 *
 * 原子地为 sfd 中所有有效 fd 注册 accept_handler 可读事件。
 * 若任意 fd 注册失败，回滚已注册的所有事件并返回 SERVER_ERR。
 * @param server         服务器指针（持有事件循环）
 * @param sfd            监听 socket 集合
 * @param accept_handler TCP 接受连接的事件回调函数
 * @param privdata       透传给回调的用户数据（通常为 server 指针）
 * @return int 全部成功返回 SERVER_OK，任意失败返回 SERVER_ERR
 */
int createSocketAcceptHandler(struct latte_server_t* server, socket_fds_t *sfd, ae_file_proc_func *accept_handler, void* privdata) {
    int j;
    for (j = 0; j < sfd->count; j++) {
        if (ae_file_event_new(server->el, sfd->fd[j], AE_READABLE, accept_handler, privdata) == AE_ERR) {
            /* Rollback */
            for (j = j-1; j >= 0; j--) ae_file_event_delete(server->el, sfd->fd[j], AE_READABLE);
            return SERVER_ERR;
        }
    }
    return SERVER_OK;
}

/**
 * @brief 写事件处理器：将输出缓冲区数据发送给客户端
 * 从 conn 的私有数据中取出 latte_client_t，调用 write_to_client。
 * @param conn 触发写事件的连接对象
 */
void sendReplyToClient(connection *conn) {
    latte_client_t *c = connGetPrivateData(conn);
    write_to_client(c,1);
}


/**
 * @brief 在进入事件循环前处理所有待写客户端，尽量避免安装写事件处理器
 *
 * 先等待所有异步 IO 写操作完成，再遍历 clients_pending_write 队列：
 * - 跳过受保护或即将关闭的客户端
 * - 尝试同步写入，若仍有待发数据则安装写事件处理器
 *
 * @param server 服务器指针
 * @return int 处理的客户端数量
 */
int handleClientsWithPendingWrites(latte_server_t* server) {
    
    while (server->clients_async_pending_write != NULL 
        && list_length(server->clients_async_pending_write) != 0) {
        LATTE_LIB_LOG(LOG_DEBUG, "handleClientsWithPendingWrites async_io_each_finished");
        async_io_each_finished();
    }
    
    list_iterator_t li;
    list_node_t* ln;
    int processed = list_length(server->clients_pending_write);
    if (processed == 0) return 0;
    LATTE_LIB_LOG(LOG_DEBUG, "handleClientsWithPendingWrites processed %d", processed);
    // LATTE_LIB_LOG(LL_DEBUG, "processed %d", processed);
    list_rewind(server->clients_pending_write, &li);
    while((ln = list_next(&li))) {
        latte_client_t *c = list_node_value(ln);
        c->flags &= ~CLIENT_PENDING_WRITE;
        list_del_node(server->clients_pending_write,ln);

        /* If a client is protected, don't do anything,
         * that may trigger write error or recreate handler. */
        if (c->flags & CLIENT_PROTECTED) {
            LATTE_LIB_LOG(LOG_DEBUG, "CLIENT_PROTECTED (fd)%d (flags)%d", c->conn->fd, c->flags);
            continue;
        }

        /* Don't write to clients that are going to be closed anyway. */
        if (c->flags & CLIENT_CLOSE_ASAP) {
            LATTE_LIB_LOG(LOG_DEBUG, "CLIENT_CLOSE_ASAP (fd)%d (flags)%d", c->conn->fd, c->flags);
            continue;
        }

        /* Try to write buffers to the client socket. */
        if (write_to_client(c,0) == -1) continue;

        /* If after the synchronous writes above we still have data to
         * output to the client, we need to install the writable handler. */
        if (client_has_pending_replies(c)) {
            int ae_barrier = 0;
            /* For the fsync=always policy, we want that a given FD is never
             * served for reading and writing in the same event loop iteration,
             * so that in the middle of receiving the query, and serving it
             * to the client, we'll call beforeSleep() that will do the
             * actual fsync of AOF to disk. the write barrier ensures that. */
            // if (server.aof_state == AOF_ON &&
            //     server.aof_fsync == AOF_FSYNC_ALWAYS)
            // {
            //     ae_barrier = 1;
            // }
            if (conn_set_write_handlerWithBarrier(server->el, c->conn, sendReplyToClient, ae_barrier) == -1) {
                free_latte_client_async(c);
            }
        }
    }
    return processed;
}

/**
 * @brief before-sleep 任务函数：在事件循环睡眠前发送待写客户端数据
 * @param task 任务结构体，args[0] 为 latte_server_t 指针
 * @return int 始终返回 1
 */
int send_clients(latte_func_task_t* task) {
    latte_server_t* server = (latte_func_task_t*)task->args[0];
    handleClientsWithPendingWrites(server);
    return 1;
}

/**
 * @brief 服务器定时任务回调（注册到事件循环的时间事件）
 * 每 1ms 触发一次，驱动 cron_manager 执行所有到期的定时任务。
 * @param eventLoop 事件循环指针（未使用）
 * @param id        时间事件 ID（未使用）
 * @param clientData latte_server_t 指针
 * @return int 返回 1 表示继续触发（每 1ms 重复）
 */
int serverCron(struct ae_event_loop_t *eventLoop, long long id, void *clientData) {
    // LATTE_LIB_LOG(LOG_INFO, "serverCron");
    latte_server_t* server = (latte_server_t*)clientData;
    cron_manager_run(server->cron_manager, server);
    return 1;
}

/**
 * @brief 启动 latte 服务器
 *
 * 执行顺序：
 *   1. 监听端口（listenToPort）
 *   2. 检查监听 socket 是否有效
 *   3. 注册 TCP 接受连接事件（createSocketAcceptHandler）
 *   4. 注册 before-sleep 任务（发送待写客户端数据）
 *   5. 注册定时任务（serverCron）
 *   6. 进入事件循环（ae_main），直到 ae_stop 被调用
 *
 * 任何关键步骤失败均调用 exit(1) 终止进程。
 * @param server 已调用 init_latte_server 初始化的服务器指针
 * @return int 事件循环退出后返回 1
 */
int start_latte_server(latte_server_t* server) {
    LATTE_LIB_LOG(LOG_INFO, "start latte server %lld %lld !!!!!", server->port, server->tcp_backlog);
    //listen port
    if (server->port != 0 &&
        listenToPort(server, server->neterr,server->bind,server->port,server->tcp_backlog,&server->ipfd) == -1) {
        LATTE_LIB_LOG(LOG_ERROR,"start latte server fail!!!");
        exit(1);
    }
    //TODO listening tls port
    {}
    //TODO listening Unix
    {}
    /* Abort if there are no listening sockets at all. */
    if (server->ipfd.count == 0 
        // && server.tlsfd.count == 0 
        // && server.sofd < 0
    ) {
        LATTE_LIB_LOG(LOG_ERROR, "Configured to not listen anywhere, exiting.");
        exit(1);
    }
    if (server->acceptTcpHandler == NULL) {
        LATTE_LIB_LOG(LOG_ERROR, "server unset acceptTcpHandler");
        exit(1);
    }

    /* Create an event handler for accepting new connections in TCP and Unix
     * domain sockets. */
    if (createSocketAcceptHandler(server, &server->ipfd, server->acceptTcpHandler, server) != SERVER_OK) {
        LATTE_LIB_LOG(LOG_ERROR, "Unrecoverable error creating TCP socket accept handler.");
        exit(1);
    }
    ae_add_before_sleep_task(server->el, latte_func_task_new(send_clients, NULL, 1, server));
    LATTE_LIB_LOG(LOG_INFO, "start latte server success PID: %lld" , getpid());

    /* Create the timer callback, this is our way to process many background
     * operations incrementally, like clients timeout, eviction of unaccessed
     * expired keys and so forth. */
    if (ae_time_event_new(server->el, 1, serverCron, server, NULL) == AE_ERR) {
        // serverPanic("Can't create event loop timers.");
        exit(1);
    }
    ae_main(server->el);
    ae_event_loop_delete(server->el);
    return 1;
}
/**
 * @brief 停止 latte 服务器：执行 before-sleep 任务后停止事件循环
 * @param server 目标服务器指针
 * @return int 始终返回 1
 */
int stop_latte_server(struct latte_server_t* server) {
    ae_do_before_sleep(server->el);
    ae_stop(server->el);
    return 1;
}




/**
 * @brief 从已接受的连接创建并注册客户端（内部通用处理函数）
 *
 * 检查连接状态、客户端数量上限，通过 createClient 工厂分配客户端，
 * 设置连接属性（非阻塞、TCP_NODELAY、读事件），最后链接到服务器。
 *
 * @param server 服务器指针
 * @param conn   已接受的连接对象
 * @param flags  初始客户端标志
 * @param ip     客户端 IP 字符串（仅用于调试日志）
 */
static void acceptCommonHandler(latte_server_t* server,connection *conn, int flags, char *ip) {
    struct latte_client_t *c;
    char conninfo[100];
    UNUSED(ip);
    if (conn_get_state(conn) != CONN_STATE_ACCEPTING) {
        LATTE_LIB_LOG(LOG_ERROR, "Accepted client connection in error state: %s (conn: %s)",
            conn_get_last_error(conn),
            connGetInfo(conn, conninfo, sizeof(conninfo)));
        connClose(server->el,conn);
        return;
    }
    /* Limit the number of connections we take at the same time.
     *
     * Admission control will happen before a client is created and connAccept()
     * called, because we don't want to even start transport-level negotiation
     * if rejected. */
    //  + getClusterConnectionsCount()
    if (list_length(server->clients)
        >= server->maxclients)
    {
        char *err = "-ERR max number of clients reached\r\n";

        /* That's a best effort error message, don't check write errors.
         * Note that for TLS connections, no handshake was done yet so nothing
         * is written and the connection will just drop. */
        if (conn_write(conn,err,strlen(err)) == -1) {
            /* Nothing to do, Just to avoid the warning... */
        }
        // server.stat_rejected_conn++;
        connClose(server->el, conn);
        return;
    }
    /* Create connection and client */
    c = server->createClient();
    if (c == NULL) {
        LATTE_LIB_LOG(LOG_ERROR, "Error registering fd event for the new client: %s (conn: %s)\n",
            conn_get_last_error(conn),
            connGetInfo(conn, conninfo, sizeof(conninfo)));
        connClose(server->el, conn); /* May be already closed, just ignore errors */
        return;
    }
    protected_init_latte_client(c);
    
    c->conn = conn;
    c->id = getClientId(server);
    c->server = server;
    LATTE_LIB_LOG(LOG_DEBUG,"create client fd:%d", c->conn->fd);

    if (conn) {
        connNonBlock(conn);         // 设置非阻塞
        connEnableTcpNoDelay(conn); // 设置TCP_NODELAY
        // if (server.tcpkeepalive)
        //     connKeepAlive(conn,server.tcpkeepalive);
        conn_set_read_handler(server->el, conn, read_query_from_client);
        connSetPrivateData(conn, c);
    }
    init_latte_client(server->el, c, conn, flags);
    if(conn) link_latte_client(server, c);
}

/** @brief 单次 accept 最大连接数：防止单个可读事件占用过长时间 */
#define MAX_ACCEPTS_PER_CALL 1000
/** @brief IP 字符串最大长度（IPv6 INET6_ADDRSTRLEN = 46） */
#define NET_IP_STR_LEN 46 /* INET6_ADDRSTRLEN is 46, but we need to be sure */

/**
 * @brief 可读事件回调：批量接受 TCP 新连接（最多 MAX_ACCEPTS_PER_CALL 次）
 *
 * 循环调用 anetTcpAccept 获取新 fd，创建连接对象后
 * 调用 acceptCommonHandler 完成客户端初始化。
 * EWOULDBLOCK 表示无更多连接，正常返回。
 *
 * @param el       事件循环指针（未使用）
 * @param fd       监听 socket fd
 * @param privdata latte_server_t 指针
 * @param mask     事件掩码（未使用）
 */
void acceptTcpHandler(ae_event_loop_t *el, int fd, void *privdata, int mask) {
    int cport, cfd, max = MAX_ACCEPTS_PER_CALL;
    char cip[NET_IP_STR_LEN];
    latte_server_t* server = (latte_server_t*)(privdata);
    while(max--) {
        cfd = anetTcpAccept(server->neterr, fd, cip, sizeof(cip), &cport);
        if (cfd == ANET_ERR) {
            if (errno != EWOULDBLOCK)
                LATTE_LIB_LOG(LOG_ERROR,
                    "Accepting client connection: %s", server->neterr);
            return;
        }
        anetCloexec(cfd);
        connection * c = connCreateAcceptedSocket(cfd);
        LATTE_LIB_LOG(LOG_DEBUG ,"Accepted %s:%d %d", cip, cport, cfd);
        acceptCommonHandler(server, c, 0, cip);
    }
}

/**
 * @brief 初始化 latte 服务器内部状态
 *
 * 创建客户端链表、索引基数树、待写队列、定时任务管理器，
 * 并设置默认的 TCP 接受处理函数。
 * 调用方需在此之前完成 el、port、bind 等字段的赋值。
 * @param server 目标服务器指针
 */
void init_latte_server(latte_server_t* server) {
    server->clients = list_new();
    server->clients_index = raxNew();
    server->acceptTcpHandler = acceptTcpHandler;
    server->next_client_id = 0;
    server->clients_pending_write = list_new();
    server->cron_manager = cron_manager_new();
    server->use_async_io = false;
    server->ipfd.count = 0;
    server->clients_async_pending_write = list_new();
}

/**
 * @brief 销毁 latte 服务器，释放所有内部资源
 *
 * 释放客户端链表、客户端索引基数树、定时任务管理器，
 * 并将相关指针置 NULL。不负责释放 server 指针本身。
 * @param server 目标服务器指针
 */
void destory_latte_server(latte_server_t* server) {
    if (server->clients != NULL) {
        list_delete(server->clients);
        server->clients = NULL;
    }
    if (server->clients_index != NULL) {
        raxFree(server->clients_index);
        server->clients_index = NULL;
    }
    if (server->cron_manager != NULL) {
        cron_manager_delete(server->cron_manager);
        server->cron_manager = NULL;
    }
    server->acceptTcpHandler = NULL;
}


