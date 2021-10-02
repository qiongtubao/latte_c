#include "latteServer.h"
#include "anet.h"
#include <errno.h>
#include "util.h"
#include <unistd.h>
// ConnectionType baseConnectionType = {
//     .ae_handler = connSocketEventHandler,
//     .close = connSocketClose,
//     .write = connSocketWrite,
//     .read = connSocketRead,
//     .accept = connSocketAccept,
//     .connect = connSocketConnect,
//     .set_write_handler = connSocketSetWriteHandler,
//     .set_read_handler = connSocketSetReadHandler,
//     .get_last_error = connSocketGetLastError,
//     .blocking_connect = connSocketBlockingConnect,
//     .sync_write = connSocketSyncWrite,
//     .sync_read = connSocketSyncRead,
//     .sync_readline = connSocketSyncReadLine,
//     .get_type = connSocketGetType
// };
#define PROTO_IOBUF_LEN (1024*16)
void readQueryFromClient(connection *conn) {
    printf("[read] run1\n");
    latteClient *c = connGetPrivateData(conn);
    int nread, readlen;
    size_t qblen;
    char querybuf[PROTO_IOBUF_LEN];
    readlen = PROTO_IOBUF_LEN;
    nread = connRead(c->conn, querybuf, readlen);
    printf("[readQueryFromClient] len:%d, data:%s\r\n", nread, querybuf);
}

latteClient* createClient(connection* conn) {
    latteClient *c = zmalloc(sizeof(latteClient));
    c->conn = conn;
    if(conn) {
        connNonBlock(conn);
        connEnableTcpNoDelay(conn);
        // if(tcpkeepalive) {
        // connKeepAlive(conn,server.tcpkeepalive);
        // }
        connSetReadHandler(conn, readQueryFromClient);
        connSetPrivateData(conn, c);
    }
    return c;
}

latteServer* createServer(int port) {
    latteServer* server = zmalloc(sizeof(*server));
    server->el = aeCreateEventLoop(1024);
    // server->clients = NULL;
    server->createClient = createClient;
    server->port = port;
    server->type = NULL;
    server->fd = -1;
    return server;
}


void freeClient(latteClient* c) {

}

void freeClientAsync(latteClient* c) {

}

#define MAX_ACCEPTS_PER_CALL 1000
#define NET_IP_STR_LEN 46
void clientAcceptHandler(connection *conn) {
    latteClient* c = connGetPrivateData(conn);
    if (connGetState(conn) != CONN_STATE_CONNECTED) {
        // serverLog(LL_WARNING,
        //         "Error accepting a client connection: %s",
        //         connGetLastError(conn));
        freeClientAsync(c);
        return;
    }
    printf("[clientAcceptHandler] \n");
    /*?????????????*/
}
static void acceptCommonHandler(latteServer* server, connection *conn, int flags, char *ip) {
    latteClient *c;
    char conninfo[100];
    // UNUSED(ip);
    if (connGetState(conn) != CONN_STATE_ACCEPTING) {
        // latteLog(LL_VERBOSE,
        //     "Accepted client connection in error state: %s (conn: %s)",
        //     connGetLastError(conn),
        //     connGetInfo(conn, conninfo, sizeof(conninfo)));
        // connClose(conn);
        return;
    }
    if ((c = server->createClient(conn)) == NULL) {
        // serverLog(LL_WARNING,
        //     "Error registering fd event for the new client: %s (conn: %s)",
        //     connGetLastError(conn),
        //     connGetInfo(conn, conninfo, sizeof(conninfo)));
        connClose(conn); /* May be already closed, just ignore errors */
        return;
    }
    c->flags |= flags;
    if (connAccept(conn, clientAcceptHandler) == C_ERR) {
        char conninfo[100];
        if (connGetState(conn) == CONN_STATE_ERROR)
            // serverLog(LL_WARNING,
            //         "Error accepting a client connection: %s (conn: %s)",
            //         connGetLastError(conn), connGetInfo(conn, conninfo, sizeof(conninfo)));
        freeClient(connGetPrivateData(conn));
        return;
    }

}

void acceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
    int cport, cfd, max = MAX_ACCEPTS_PER_CALL;
    char cip[NET_IP_STR_LEN];
    latteServer* server = (latteServer*)privdata;
    while(max--) {
        printf("anetTcpAccept\n");
        cfd = anetTcpAccept(server->neterr, fd, cip, sizeof(cip), &cport);
        printf("cfd: %d\n", cfd);
        if (cfd == ANET_ERR) {
            if (errno != EWOULDBLOCK) {
                // serverLog(LL_WARNING,
                //     "Accepting client connection: %s", server.neterr);
            }
            return;
        }
        // serverLog(LL_VERBOSE,"Accepted %s:%d", cip, cport);
        acceptCommonHandler(server, connCreateAcceptedSocket(server->el, server->type, cfd),0,cip);
        printf("[%d]???\n", (int)getpid());
    }
}

int runServer(latteServer* server) {
    if(server->fd != -1) {
        // latteLog(LL_WARNING, "server runing...");
        return 0;
    }
    int fd = anetTcpServer(server->neterr, server->port, NULL,
                    511);
    anetNonBlock(NULL, fd);
    server->fd = fd;
    if (aeCreateFileEvent(server->el, server->fd, AE_READABLE, acceptTcpHandler, server) == AE_ERR) {
        // latteLog(
        //             "Unrecoverable error creating server.ipfd file event.");
        return 0;
    }
    return 1;
}

#if defined(SERVER_TEST_MAIN)
#include "connection.h"
void sendPingCommand(connection *conn) {
    sds cmd = sdscatprintf(sdsempty(),"PING\r\n");
    printf("send ping command \n");
    if(connSyncWrite(conn, cmd, sdslen(cmd),1000) == -1) {
        sdsfree(cmd);
        printf("-Writing to master: %s",
                connGetLastError(conn));
        return;
    }
    sdsfree(cmd);
    return;
}

int main() {
    int childpid;
    if ((childpid = fork()) == 0) {
        /* Child */
        struct aeEventLoop *eventLoop = aeCreateEventLoop(128);
        connection * conn = connCreateSocket(eventLoop, &CT_Socket);
        printf("hello\r\n");
        if (connConnect(conn, "127.0.0.1", 6379,
                NULL, sendPingCommand) == C_ERR) {
            return 0;
        }
    } else {
        /* Parent */
        //启动server
        latteServer* server = createServer(6379);
        runServer(server);
        aeMain(server->el);
    }
}
#endif