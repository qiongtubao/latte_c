#include "server.h"
#include <errno.h>
#include <string.h>
#include "utils/atomic.h"

uint64_t getClientId(struct latteServer* server) {
    uint64_t client_id;
    atomicGetIncr(server->next_client_id, client_id, 1);
    return client_id;
}

void closeSocketListeners(struct latteServer* server, socketFds *sfd) {
    int j;

    for (j = 0; j < sfd->count; j++) {
        if (sfd->fd[j] == -1) continue;

        aeDeleteFileEvent(server->el, sfd->fd[j], AE_READABLE);
        close(sfd->fd[j]);
    }

    sfd->count = 0;
}

//监听端口
int listenToPort(struct latteServer* server,char* neterr, struct arraySds* bind, int port, int tcp_backlog, socketFds *sfd) {
    int j;
    char** bindaddr;
    int bindaddr_count;
    char *default_bindaddr[2] = {"*", "-::*"};
    if (bind == NULL 
        || (bind != NULL && bind->len == 0)) {
        bindaddr = default_bindaddr;
        bindaddr_count = 2;
    } else {
        bindaddr = (char**)bind->value;
        bindaddr_count = bind->len;
    }
    for (j = 0; j < bindaddr_count; j++) {
        char* addr = bindaddr[j];
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
    return 1;
}

/* 创建事件处理程序，用于接受 TCP 或 TLS 域套接字中的新连接。
 * 这原子地适用于所有插槽 fds */
int createSocketAcceptHandler(struct latteServer* server, socketFds *sfd, aeFileProc *accept_handler, void* privdata) {
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
int startLatteServer(struct latteServer* server) {
    log_info("latte_c","start latte server %lld %lld !!!!!", server->port, server->tcp_backlog);
    //listen port
    if (server->port != 0 &&
        listenToPort(server, server->neterr,server->bind,server->port,server->tcp_backlog,&server->ipfd) == -1) {
        log_error("latte_c","start latte server fail!!!");
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
        log_error("latte_c","Configured to not listen anywhere, exiting.");
        exit(1);
    }
    if (server->acceptTcpHandler == NULL) {
        log_error("latte_c","server unset acceptTcpHandler");
        exit(1);
    }

    /* Create an event handler for accepting new connections in TCP and Unix
     * domain sockets. */
    if (createSocketAcceptHandler(server, &server->ipfd, server->acceptTcpHandler, server) != SERVER_OK) {
        log_error("latte_c","Unrecoverable error creating TCP socket accept handler.");
        exit(1);
    }

    aeMain(server->el);
    aeDeleteEventLoop(server->el);
    return 1;
}
int stopServer(struct latteServer* server) {
    aeStop(server->el);
    return 1;
}

/**
 * @brief 
 * 
 * @param client 
 * 从可能引用客户端的全局列表中移除指定的客户端，
 * 不包括发布/订阅通道。
 * 这是由freeClient()和replicationCacheMaster()使用的。
 */
void unlinkClient(struct latteClient* c) {
    struct latteServer* server = c->server;
    listNode *ln;
    if (c->conn) {
        if (c->client_list_node) {
            uint64_t id = htonu64(c->id);
            raxRemove(server->clients_index,(unsigned char*)&id,sizeof(id),NULL);
            listDelNode(server->clients,c->client_list_node);
            c->client_list_node = NULL;
        }
        connClose(server->el, c->conn);
        c->conn = NULL;
    }
}

void freeClient(struct latteClient *c) {
    log_debug("latte_c","freeClient %d\n", c->conn->fd);
    struct latteServer* server = c->server;
    unlinkClient(c);
    freeInnerLatteClient(c);
    server->freeClient(c);
}

/* 放入异步删除队列 */
void freeClientAsync(struct latteClient *c) {
    struct latteServer* server = c->server;
    //TODO
    freeClient(c);
}
void clientAcceptHandler(connection *conn) {
    struct latteClient *c = connGetPrivateData(conn);

    if (connGetState(conn) != CONN_STATE_CONNECTED) {
        log_error("latte_c", "Error accepting a client connection: %s\n", connGetLastError(conn));
        freeClientAsync(c);
        return;
    }
}

void initClient(struct aeEventLoop* el,struct latteClient* c, struct connection* conn, int flags) {
    /* Last chance to keep flags */
    c->flags |= flags;
    /* Initiate accept.
     *
     * Note that connAccept() is free to do two things here:
     * 1. Call clientAcceptHandler() immediately;
     * 2. Schedule a future call to clientAcceptHandler().
     *
     * Because of that, we must do nothing else afterwards.
     */
    if (connAccept(el, conn, clientAcceptHandler) == CONNECTION_ERR) {
        char conninfo[100];
        if (connGetState(conn) == CONN_STATE_ERROR) {
            log_error("latte_c", "Error accepting a client connection: %s (conn: %s)\n", 
                connGetLastError(conn), 
                connGetInfo(conn, conninfo, sizeof(conninfo))
            );
        }
        freeClient(connGetPrivateData(conn));
        return;
    }
}

void linkClient(struct latteServer* server, struct latteClient* c) {
    listAddNodeTail(server->clients, c);
    /* Note that we remember the linked list node where the client is stored,
     * this way removing the client in unlinkClient() will not require
     * a linear scan, but just a constant time operation. */
    /* 请注意，我们记住了存储客户端的链表节点，这样在 unlinkClient() 中移除客户端将不需要进行线性扫描，而只需要进行一个常数时间的操作。 */
    c->client_list_node = listLast(server->clients);
    uint64_t id = htonu64(c->id);
    raxInsert(server->clients_index,(unsigned char*)&id,sizeof(id),c,NULL);
}


#define PROTO_REQ_INLINE 1
#define PROTO_REQ_MULTIBULK 2

#define PROTO_IOBUF_LEN         (1024*16)  /* Generic I/O buffer size */
#define PROTO_REPLY_CHUNK_BYTES (16*1024) /* 16k output buffer */
#define PROTO_INLINE_MAX_SIZE   (1024*64) /* Max size of inline reads */
#define PROTO_MBULK_BIG_ARG     (1024*32)
void readQueryFromClient(connection *conn) {
    struct latteClient *c = connGetPrivateData(conn);
    int nread, readlen;
    size_t qblen;
    /**
     * @brief Construct a new if object
     *  在这个代码块中，如果启用了线程化I/O，
     *  就会在事件循环之前从客户端套接字读取数据。
     *  这是因为线程化I/O需要在I/O线程中处理客户端套接字的读写操作，
     *  而主线程只负责事件循环。
     *  因此，在进入事件循环之前，
     *  需要从客户端套接字读取数据，
     *  以便I/O线程能够处理它们。
     */
    // if (postponeClientRead(c)) return;
    readlen = PROTO_IOBUF_LEN;
    /**
     * @brief 
     * 这个代码块是用于解析客户端发送的Redis协议请求的。
     * 在处理多个批量请求时，
     * 如果当前正在处理一个足够大的批量回复，
     * 就会尝试尽可能地让查询缓冲区包含表示对象的SDS字符串，
     * 即使这可能需要更多的`read(2)`调用。
     * 这样可以避免在处理批量请求时创建新的SDS字符串，
     * 从而提高性能。
     * 
     */
    qblen = sdslen(c->querybuf);
    if (c->querybuf_peak < qblen) c->querybuf_peak = qblen;
    c->querybuf = sdsMakeRoomFor(c->querybuf, readlen);
    
    nread = connRead(c->conn, c->querybuf + qblen, readlen);
    if (nread == -1) {
        if (connGetState(conn) == CONN_STATE_CONNECTED) {
            return;
        } else {
            log_error("latte_c", "Reading from client: %s\n",connGetLastError(c->conn));
            freeClientAsync(c);
            return;
        }
    } else if (nread == 0) {
        freeClientAsync(c);
        return;
    } 
    sdsIncrLen(c->querybuf,nread);
    if (c->exec(c)) {
        //清理掉c->querybuf
        sdsrange(c->querybuf,c->qb_pos,-1);
        c->qb_pos = 0;
    }
}


//这里从connection 创建client
static void acceptCommonHandler(struct latteServer* server,connection *conn, int flags, char *ip) {
    latteClient *c;
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
    if (listLength(server->clients)
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
    initInnerLatteClient(c);
    
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
    initClient(server->el, c, conn, flags);
    if(conn) linkClient(server, c);
}

#define MAX_ACCEPTS_PER_CALL 1000
#define NET_IP_STR_LEN 46 /* INET6_ADDRSTRLEN is 46, but we need to be sure */
void acceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
    int cport, cfd, max = MAX_ACCEPTS_PER_CALL;
    char cip[NET_IP_STR_LEN];
    struct latteServer* server = (struct latteServer*)(privdata);

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

void initInnerLatteServer(struct latteServer* server) {
    server->clients = listCreate();
    server->clients_index = raxNew();
    server->acceptTcpHandler = acceptTcpHandler;
    server->next_client_id = 0;
}
void freeInnerLatteServer(struct latteServer* server) {
    if (server->clients != NULL) {
        listRelease(server->clients);
        server->clients = NULL;
    }
    if (server->clients_index != NULL) {
        raxFree(server->clients_index);
        server->clients_index = NULL;
    }
    server->acceptTcpHandler = NULL;
}

/**client **/
void initInnerLatteClient(struct latteClient* client) {
    client->qb_pos = 0;
    client->querybuf = sdsempty();
    client->querybuf_peak = 0;
    client->client_list_node = NULL;
}

void freeInnerLatteClient(struct latteClient* client) {
    if (client->querybuf != NULL) {
        sdsfree(client->querybuf);
        client->querybuf = NULL;
    }
}
