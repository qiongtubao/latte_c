

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

/* Error codes */
#define SERVER_OK                    0
#define SERVER_ERR                   -1

#define CONFIG_BINDADDR_MAX 16

#define ANET_ERR_LEN 256

/***************** latteServer *****************/
struct latte_client_t;
typedef struct socket_fds_t {
    int fd[CONFIG_BINDADDR_MAX];
    int count;
} socket_fds_t;

typedef void (*tcp_handler_func)(aeEventLoop *el, int fd, void *privdata, int mask);
// typedef int (*exec_func)(struct latte_client_t* client);
typedef struct latte_client_t *(*create_client_func)();
typedef void (*free_client_func)(struct latte_client_t* client);
typedef struct latte_server_t {
    /* General */
    pid_t pid;  
    pthread_t main_thread_id;
    aeEventLoop *el;
    long long port;
    socket_fds_t ipfd;
    vector_t* bind;
    char neterr[ANET_ERR_LEN];   /* Error buffer for anet.c */
    long long tcp_backlog;
    tcp_handler_func acceptTcpHandler;
    create_client_func createClient;
    free_client_func freeClient;
    latteAtomic uint64_t next_client_id;
    list_t *clients;
    unsigned int maxclients;
    rax *clients_index;         /* Active clients dictionary by client ID. */
} latte_server_t;



int start_latte_server(struct latte_server_t* server);
int stop_latte_server(struct latte_server_t* server);
void init_latte_server(struct latte_server_t* server);
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

void readQueryFromClient(connection *conn);
#endif