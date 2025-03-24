#include "client.h"

sds_t latte_client_info_to_sds(latte_client_t* client);



/**client **/
void protected_init_latte_client(latte_client_t* client) {
    client->qb_pos = 0;
    client->querybuf = sds_empty();
    client->querybuf_peak = 0;
    client->client_list_node = NULL;
}


void clientAcceptHandler(connection *conn) {
    latte_client_t *c = connGetPrivateData(conn);

    if (connGetState(conn) != CONN_STATE_CONNECTED) {
        log_error("latte_c", "Error accepting a client connection: %s\n", connGetLastError(conn));
        free_latte_client_async(c);
        return;
    }
}

void init_latte_client(struct aeEventLoop* el, latte_client_t* c, struct connection* conn, int flags) {
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
        free_latte_client(connGetPrivateData(conn));
        return;
    }
}

void destory_latte_client(latte_client_t* client) {
    if (client->querybuf != NULL) {
        sds_delete(client->querybuf);
        client->querybuf = NULL;
    }
}



void link_latte_client(latte_server_t* server, latte_client_t* c) {
    list_add_node_tail(server->clients, c);
    /* 请注意，我们记住了存储客户端的链表节点，这样在 unlinkClient() 中移除客户端将不需要进行线性扫描，而只需要进行一个常数时间的操作。 */
    c->client_list_node = list_last(server->clients);
    uint64_t id = htonu64(c->id);
    raxInsert(server->clients_index,(unsigned char*)&id,sizeof(id),c,NULL);
}
/**
 * @brief 
 * 
 * @param client 
 * 从可能引用客户端的全局列表中移除指定的客户端，
 * 不包括发布/订阅通道。
 * 这是由freeClient()和replicationCacheMaster()使用的。
 */
void unlink_latte_client(latte_client_t* c) {
    latte_server_t* server = c->server;
    list_node_t *ln;
    if (c->conn) {
        if (c->client_list_node) {
            uint64_t id = htonu64(c->id);
            raxRemove(server->clients_index,(unsigned char*)&id,sizeof(id),NULL);
            list_del_node(server->clients,c->client_list_node);
            c->client_list_node = NULL;
        }
        connClose(server->el, c->conn);
        c->conn = NULL;
    }
}

void free_latte_client(latte_client_t *c) {
    log_debug("latte_c","freeClient %d\n", c->conn->fd);
    latte_server_t* server = c->server;
    unlink_latte_client(c);
    destory_latte_client(c);
    server->freeClient(c);
}

/* 放入异步删除队列 */
void free_latte_client_async(latte_client_t *c) {
    struct latte_server_t* server = c->server;
    //TODO
    free_latte_client(c);
}

void readQueryFromClient(connection *conn) {
    latte_client_t *c = connGetPrivateData(conn);
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
    qblen = sds_len(c->querybuf);
    if (c->querybuf_peak < qblen) c->querybuf_peak = qblen;
    c->querybuf = sds_make_room_for(c->querybuf, readlen);
    
    nread = connRead(c->conn, c->querybuf + qblen, readlen);
    if (nread == -1) {
        if (connGetState(conn) == CONN_STATE_CONNECTED) {
            return;
        } else {
            log_error("latte_c", "Reading from client: %s\n",connGetLastError(c->conn));
            free_latte_client_async(c);
            return;
        }
    } else if (nread == 0) {
        free_latte_client_async(c);
        return;
    } 
    sds_incr_len(c->querybuf,nread);
    if (c->exec(c)) {
        //清理掉c->querybuf
        sds_range(c->querybuf,c->qb_pos,-1);
        c->qb_pos = 0;
    }
}


int _add_reply_to_buffer(latte_client_t* c, const char* s, size_t len) {
    size_t available = sizeof(c->buf)-c->bufpos;

    //已经关闭的client 直接返回成功
    if (c->flags & CLIENT_CLOSE_AFTER_REPLY) return 0;
    //如果排队等待发送的队列里还有数据 就排到队里后面
    if (list_length(c->reply) > 0) return -1;
    //内存不够 排队到后面的reply
    if (len > available) return -1;

    memcpy(c->buf +c->bufpos,s,len);
    c->bufpos += len;
    return 0;
}

void _add_reply_proto_to_list(latte_client_t* c, const char* s, size_t len) {
    if (c->flags & CLIENT_CLOSE_AFTER_REPLY) return ;

    list_node_t* ln = list_last(c->reply);
    client_reply_block_t* tail = ln? list_node_value(ln): NULL;

    if (tail) {
        size_t avail = tail->size - tail->used;
        size_t copy = avail >= len? len: avail;
        memcpy(tail->buf + tail->used, s, copy);
        tail->used += copy;
        s += copy;
        len -= copy;
    }
    if (len) {
        size_t size = len < PROTO_REPLY_CHUNK_BYTES? PROTO_REPLY_CHUNK_BYTES: len;
        tail = zmalloc(size + sizeof(client_reply_block_t));
        /* take over the allocation's internal fragmentation */
        tail->size = zmalloc_usable_size(tail) - sizeof(client_reply_block_t);
        tail->used = len;
        memcpy(tail->buf, s, len);
        list_add_node_tail(c->reply, tail);
        c->reply_bytes += tail->size;
        //检查是否超过内存
        // closeClientOnOutputBufferLimitReached(c, 1);
    }
}

void add_reply_proto(latte_client_t* c, const char* s, size_t len) {
    if (_add_reply_to_buffer(c,s,len) != 0)
        _add_reply_proto_to_list(c,s,len);
}