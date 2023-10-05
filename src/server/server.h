

#ifndef __LATTE_SERVER_H
#define __LATTE_SERVER_H

#include "ae/ae.h"
#include "connection/connection.h"
#include "sds/sds.h"
#include "dict/dict.h"
#include "ae/ae.h"
#include "anet/anet.h"
#include <pthread.h>
#include "sds/sds.h"
#include "config/config.h"
#include "list/list.h"
#include "endianconv/endianconv.h"
#include "rax/rax.h"
#include <stdio.h>
#include "utils/atomic.h"

/* Error codes */
#define SERVER_OK                    0
#define SERVER_ERR                   -1

#define CONFIG_BINDADDR_MAX 16

#define ANET_ERR_LEN 256

/***************** latteServer *****************/
struct latteClient;
typedef struct socketFds {
    int fd[CONFIG_BINDADDR_MAX];
    int count;
} socketFds;

typedef void *(*tcpHandlerFunc)(aeEventLoop *el, int fd, void *privdata, int mask);
typedef int *(*ExecFunc)(struct latteClient* client);
typedef struct latteClient *(*createClientFunc)();
typedef void *(*freeClientFunc)(struct latteClient* client);
typedef struct latteServer {
    /* General */
    pid_t pid;  
    pthread_t main_thread_id;
    aeEventLoop *el;
    long long port;
    socketFds ipfd;
    struct arraySds* bind;
    char neterr[ANET_ERR_LEN];   /* Error buffer for anet.c */
    long long tcp_backlog;
    tcpHandlerFunc acceptTcpHandler;
    createClientFunc createClient;
    freeClientFunc freeClient;
    latteAtomic uint64_t next_client_id;
    list *clients;
    unsigned int maxclients;
    rax *clients_index;         /* Active clients dictionary by client ID. */
} latteServer;



int startLatteServer(struct latteServer* server);
int stopServer(struct latteServer* server);
void initInnerLatteServer(struct latteServer* server);
void freeInnerLatteServer(struct latteServer* server);



/* latteClient */

typedef int *(*ExecHandler)(struct latteClient* client);
typedef struct latteClient {
    uint64_t id;
    connection *conn;
    sds querybuf;    /* 我们用来累积客户端查询的缓冲区 */
    size_t qb_pos;
    size_t querybuf_peak;   /* 最近（100毫秒或更长时间）查询缓冲区大小的峰值 */
    ExecFunc exec;
    int flags;
    struct latteServer* server;
    struct listNode* client_list_node;
} latteClient;

void initInnerLatteClient(struct latteClient* client);
void freeInnerLatteClient(struct latteClient* client);

#endif