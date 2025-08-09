#include "server.h"
#include <errno.h>
#include <string.h>
#include "utils/atomic.h"
#include "client.h"

uint64_t getClientId(struct latte_server_t* server) {
    uint64_t client_id;
    latte_atomic_get_incr(server->next_client_id, client_id, 1);
    return client_id;
}

void closeSocketListeners(struct latte_server_t* server, socket_fds_t *sfd) {
    int j;

    for (j = 0; j < sfd->count; j++) {
        if (sfd->fd[j] == -1) continue;

        ae_file_event_delete(server->el, sfd->fd[j], AE_READABLE);
        close(sfd->fd[j]);
    }

    sfd->count = 0;
}

//监听端口
int listenToPort(struct latte_server_t* server,char* neterr, vector_t* bind, int port, int tcp_backlog, socket_fds_t *sfd) {
    int j;
    latte_iterator_t* iterator = vector_get_iterator(bind);
    while(latte_iterator_has_next(iterator)) {
        value_t* v = latte_iterator_next(iterator);
        char* addr = value_get_sds(v);
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

/* 创建事件处理程序，用于接受 TCP 或 TLS 域套接字中的新连接。
 * 这原子地适用于所有插槽 fds */
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

/* Write event handler. Just send data to the client. */
void sendReplyToClient(connection *conn) {
    latte_client_t *c = connGetPrivateData(conn);
    writeToClient(c,1);
}


/* This function is called just before entering the event loop, in the hope
 * we can just write the replies to the client output buffer without any
 * need to use a syscall in order to install the writable event handler,
 * get it called, and so forth. */
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
        if (writeToClient(c,0) == -1) continue;

        /* If after the synchronous writes above we still have data to
         * output to the client, we need to install the writable handler. */
        if (clientHasPendingReplies(c)) {
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
            if (connSetWriteHandlerWithBarrier(server->el, c->conn, sendReplyToClient, ae_barrier) == -1) {
                free_latte_client_async(c);
            }
        }
    }
    return processed;
}

int send_clients(latte_func_task_t* task) {
    latte_server_t* server = (latte_func_task_t*)task->args[0];
    handleClientsWithPendingWrites(server);
    return 1;
}

int serverCron(struct ae_event_loop_t *eventLoop, long long id, void *clientData) {
    // LATTE_LIB_LOG(LOG_INFO, "serverCron");
    latte_server_t* server = (latte_server_t*)clientData;
    cron_manager_run(server->cron_manager, server);
    return 1;
}

//启动latte server
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
int stop_latte_server(struct latte_server_t* server) {
    ae_do_before_sleep(server->el);
    ae_stop(server->el);
    return 1;
}




//这里从connection 创建client
static void acceptCommonHandler(latte_server_t* server,connection *conn, int flags, char *ip) {
    struct latte_client_t *c;
    char conninfo[100];
    UNUSED(ip);
    if (connGetState(conn) != CONN_STATE_ACCEPTING) {
        LATTE_LIB_LOG(LOG_ERROR, "Accepted client connection in error state: %s (conn: %s)",
            connGetLastError(conn),
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
        if (connWrite(conn,err,strlen(err)) == -1) {
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
            connGetLastError(conn),
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
        connSetReadHandler(server->el, conn, read_query_from_client);
        connSetPrivateData(conn, c);
    }
    init_latte_client(server->el, c, conn, flags);
    if(conn) link_latte_client(server, c);
}

#define MAX_ACCEPTS_PER_CALL 1000
#define NET_IP_STR_LEN 46 /* INET6_ADDRSTRLEN is 46, but we need to be sure */
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
        LATTE_LIB_LOG(LOG_INFO ,"Accepted %s:%d %d", cip, cport, cfd);
        acceptCommonHandler(server, c, 0, cip);
    }
}

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


