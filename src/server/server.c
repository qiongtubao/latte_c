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

        aeDeleteFileEvent(server->el, sfd->fd[j], AE_READABLE);
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
            sfd->fd[sfd->count] = anetTcpServer(neterr,port,addr,tcp_backlog);
        }
        if (sfd->fd[sfd->count] == ANET_ERR) {
            int net_errno = errno;
            log_error("latte_c",
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
int createSocketAcceptHandler(struct latte_server_t* server, socket_fds_t *sfd, aeFileProc *accept_handler, void* privdata) {
    int j;
    for (j = 0; j < sfd->count; j++) {
        if (aeCreateFileEvent(server->el, sfd->fd[j], AE_READABLE, accept_handler, privdata) == AE_ERR) {
            /* Rollback */
            for (j = j-1; j >= 0; j--) aeDeleteFileEvent(server->el, sfd->fd[j], AE_READABLE);
            return SERVER_ERR;
        }
    }
    return SERVER_OK;
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
    LATTE_LIB_LOG(LOG_INFO, "start latte server success PID: %lld\n" , getpid());

    aeMain(server->el);
    aeDeleteEventLoop(server->el);
    return 1;
}
int stop_latte_server(struct latte_server_t* server) {
    aeStop(server->el);
    return 1;
}




//这里从connection 创建client
static void acceptCommonHandler(latte_server_t* server,connection *conn, int flags, char *ip) {
    struct latte_client_t *c;
    char conninfo[100];
    UNUSED(ip);
    if (connGetState(conn) != CONN_STATE_ACCEPTING) {
        log_error("latte_c", "Accepted client connection in error state: %s (conn: %s)",
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
        log_error("latte_c", "Error registering fd event for the new client: %s (conn: %s)\n",
            connGetLastError(conn),
            connGetInfo(conn, conninfo, sizeof(conninfo)));
        connClose(server->el, conn); /* May be already closed, just ignore errors */
        return;
    }
    protected_init_latte_client(c);
    
    c->conn = conn;
    c->id = getClientId(server);
    c->server = server;
    log_debug("latte_c","create client fd:%d", c->conn->fd);

    if (conn) {
        connNonBlock(conn);
        connEnableTcpNoDelay(conn);
        // if (server.tcpkeepalive)
        //     connKeepAlive(conn,server.tcpkeepalive);
        connSetReadHandler(server->el, conn, readQueryFromClient);
        connSetPrivateData(conn, c);
    }
    init_latte_client(server->el, c, conn, flags);
    if(conn) link_latte_client(server, c);
}

#define MAX_ACCEPTS_PER_CALL 1000
#define NET_IP_STR_LEN 46 /* INET6_ADDRSTRLEN is 46, but we need to be sure */
void acceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
    int cport, cfd, max = MAX_ACCEPTS_PER_CALL;
    char cip[NET_IP_STR_LEN];
    latte_server_t* server = (latte_server_t*)(privdata);

    while(max--) {
        cfd = anetTcpAccept(server->neterr, fd, cip, sizeof(cip), &cport);
        if (cfd == ANET_ERR) {
            if (errno != EWOULDBLOCK)
                log_error("latte_c",
                    "Accepting client connection: %s", server->neterr);
            return;
        }
        anetCloexec(cfd);
        connection * c = connCreateAcceptedSocket(cfd);
        log_debug("latte_c","Accepted %s:%d %d", cip, cport, cfd);
        acceptCommonHandler(server, c, 0, cip);
    }
}

void init_latte_server(latte_server_t* server) {
    server->clients = list_new();
    server->clients_index = raxNew();
    server->acceptTcpHandler = acceptTcpHandler;
    server->next_client_id = 0;
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
    server->acceptTcpHandler = NULL;
}


