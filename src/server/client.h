
#ifndef __LATTE_CLIENT_H
#define __LATTE_CLIENT_H

#include "server.h"

/* Client flags*/
#define CLIENT_CLOSE_AFTER_REPLY (1<<0) /* Close after writing entire reply. */
#define CLIENT_CLOSE_ASAP (1<<1)/* Close this client ASAP */
#define CLIENT_PROTECTED (1<<2) /*  Client should not be freed for now. */
#define CLIENT_PENDING_WRITE (1<<3) /* Client has output to send but a write
                                        handler is yet not installed. */

#define PROTO_REQ_INLINE 1
#define PROTO_REQ_MULTIBULK 2

#define PROTO_IOBUF_LEN         (1024*16)  /* Generic I/O buffer size */
#define PROTO_REPLY_CHUNK_BYTES (16*1024) /* 16k output buffer */
#define PROTO_INLINE_MAX_SIZE   (1024*64) /* Max size of inline reads */
#define PROTO_MBULK_BIG_ARG     (1024*32)

#define NET_MAX_WRITES_PER_EVENT (1024*64)
/* 该结构用于表示客户端的输出缓冲区，
* 它实际上是一个这样的块的链接列表，即：客户端->回复。 */
typedef struct client_reply_block_t {
    size_t size, used;
    char buf[];
} client_reply_block_t;
/* latteClient */
typedef int (*handle_func)(struct latte_client_t* client);
typedef struct latte_client_t {
    uint64_t id;
    connection *conn;
    sds_t querybuf;    /* 我们用来累积客户端查询的缓冲区 */
    size_t qb_pos;
    size_t querybuf_peak;   /* 最近（100毫秒或更长时间）查询缓冲区大小的峰值 */
    handle_func exec;
    int flags;
    struct latte_server_t* server;
    struct list_node_t* client_list_node;
    int bufpos; 
    char buf[PROTO_REPLY_CHUNK_BYTES];
    list_t* reply;          /* client_reply_block_t*/
    size_t reply_bytes;

    size_t sentlen;
} latte_client_t;

void protected_init_latte_client(latte_client_t* client);
void destory_latte_client(latte_client_t* client);
sds_t latte_client_info_to_sds(latte_client_t* client);
void add_reply_proto(latte_client_t* client, const char* data, size_t len);
void link_latte_client(latte_server_t* server, latte_client_t* c);
void unlink_latte_client(latte_client_t* c);


void init_latte_client(struct aeEventLoop* el, latte_client_t* c, struct connection* conn, int flags);
void free_latte_client_async(latte_client_t *c);
void free_latte_client(latte_client_t *c);

int writeToClient(latte_client_t *c, int handler_installed);
int clientHasPendingReplies(latte_client_t *c);
#endif