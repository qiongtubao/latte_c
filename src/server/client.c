#include "client.h"
#include <assert.h>
#include <string.h>
// #ifdef USE_ASYNC_IO
#include "async_io/async_io.h"
// #endif
sds_t latte_client_info_to_sds(latte_client_t* client);


void client_async_io_write_finished(async_io_request_t* request) {
    latte_client_t* client = request->ctx;
    list_del_node(client->server->clients_async_pending_write, client->async_io_client_node);
    client->async_io_client_node = NULL;
    request->len = -1;
    request->is_finished = false;
}

/**client **/
void protected_init_latte_client(latte_client_t* client) {
    client->qb_pos = 0;
    client->querybuf = sds_empty();
    client->querybuf_peak = 0;
    client->client_list_node = NULL;
    client->reply = list_new();
    client->reply_bytes = 0;
    client->bufpos = 0;
    client->sentlen = 0;
    client->async_io_request_cache = net_write_request_new(-1, 
        zmalloc(CLIENT_ASYNC_IO_MAX_SIZE), 
        -1, client, client_async_io_write_finished);
    client->async_io_client_node = NULL;
}


void clientAcceptHandler(connection *conn) {
    latte_client_t *c = connGetPrivateData(conn);

    if (connGetState(conn) != CONN_STATE_CONNECTED) {
        LATTE_LIB_LOG(LOG_ERROR, "Error accepting a client connection: %s\n", connGetLastError(conn));
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
            LATTE_LIB_LOG(LOG_ERROR, "Error accepting a client connection: %s (conn: %s)\n", 
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
    LATTE_LIB_LOG(LOG_DEBUG,"link_latte_client %d", c->conn->fd);
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
    LATTE_LIB_LOG(LOG_DEBUG,"unlink_latte_client %d", c->conn->fd);
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
    LATTE_LIB_LOG(LOG_DEBUG,"freeClient %d\n", c->conn->fd);
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
            LATTE_LIB_LOG(LOG_ERROR, "Reading from client: %s\n",connGetLastError(c->conn));
            free_latte_client_async(c);
            return;
        }
    } else if (nread == 0) {
        free_latte_client_async(c);
        return;
    } 
    sds_incr_len(c->querybuf,nread);
    LATTE_LIB_LOG(LOG_DEBUG, "readQueryFromClient exec %s %d", c->querybuf, nread);
    if (c->exec(c, nread)) {
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
    //TODO use async_io 
    // #ifdef USE_ASYNC_IO
    if (c->server->use_async_io && len < CLIENT_ASYNC_IO_MAX_SIZE) {
        LATTE_LIB_LOG(LOG_DEBUG, "add_reply_proto use async_io %d", len);
        memcpy(c->async_io_request_cache->buf, s, len); 
        c->async_io_request_cache->len = len;
        c->async_io_request_cache->fd = c->conn->fd;
        if (async_io_net_write(c->async_io_request_cache)) {
            list_add_node_tail(c->server->clients_async_pending_write, c);
            c->async_io_client_node = list_last(c->server->clients_async_pending_write);
            return;
        } else {
            c->async_io_request_cache->len = -1;
        }
    }
    // #endif
    LATTE_LIB_LOG(LOG_DEBUG, "add_reply_proto use buffer %d", len);
    list_add_node_tail(c->server->clients_pending_write, c);
    if (_add_reply_to_buffer(c,s,len) != 0)
        _add_reply_proto_to_list(c,s,len);
}


int clientHasPendingReplies(latte_client_t *c) {
    return c->bufpos || list_length(c->reply);
}
/* Write data in output buffers to client. Return C_OK if the client
 * is still valid after the call, C_ERR if it was freed because of some
 * error.  If handler_installed is set, it will attempt to clear the
 * write event.
 *
 * This function is called by threads, but always with handler_installed
 * set to 0. So when handler_installed is set to 0 the function must be
 * thread safe. */
int writeToClient(latte_client_t *c, int handler_installed) {
    LATTE_LIB_LOG(LOG_DEBUG, "writeToClient %d", c->conn->fd);
    /* Update total number of writes on server */
    // atomicIncr(c->server.stat_total_writes_processed, 1);
    

    ssize_t nwritten = 0, totwritten = 0;
    size_t objlen;
    client_reply_block_t *o;
    while(clientHasPendingReplies(c)) {
        if (c->bufpos > 0) {
            nwritten = connWrite(c->conn,c->buf+c->sentlen,c->bufpos-c->sentlen);
            if (nwritten <= 0) break;
            c->sentlen += nwritten;
            totwritten += nwritten;

            /* If the buffer was sent, set bufpos to zero to continue with
             * the remainder of the reply. */
            if ((int)c->sentlen == c->bufpos) {
                c->bufpos = 0;
                c->sentlen = 0;
            }
        } else {
            o = list_node_value(list_first(c->reply));
            objlen = o->used;

            if (objlen == 0) {
                c->reply_bytes -= o->size;
                list_del_node(c->reply,list_first(c->reply));
                continue;
            }
           
            nwritten = connWrite(c->conn, o->buf + c->sentlen, objlen - c->sentlen);
            if (nwritten <= 0) break;
            c->sentlen += nwritten;
            totwritten += nwritten;

            /* If we fully sent the object on head go to the next one */
            if (c->sentlen == objlen) {
                c->reply_bytes -= o->size;
                list_del_node(c->reply,list_first(c->reply));
                c->sentlen = 0;
                /* If there are no longer objects in the list, we expect
                 * the count of reply bytes to be exactly zero. */
                if (list_length(c->reply) == 0)
                    assert(c->reply_bytes == 0);
            }
        }
        /* Note that we avoid to send more than NET_MAX_WRITES_PER_EVENT
         * bytes, in a single threaded server it's a good idea to serve
         * other clients as well, even if a very large request comes from
         * super fast link that is always able to accept data (in real world
         * scenario think about 'KEYS *' against the loopback interface).
         *
         * However if we are over the maxmemory limit we ignore that and
         * just deliver as much data as it is possible to deliver.
         *
         * Moreover, we also send as much as possible if the client is
         * a slave or a monitor (otherwise, on high-speed traffic, the
         * replication/output buffer will grow indefinitely) */
        // if (totwritten > NET_MAX_WRITES_PER_EVENT &&
            // (c->server.maxmemory == 0 ||
            //  zmalloc_used_memory() < server.maxmemory) &&
            // !(c->flags & CLIENT_SLAVE)) break;
    }
    // atomicIncr(c->server.stat_net_output_bytes, totwritten);
    if (nwritten == -1) {
        if (connGetState(c->conn) != CONN_STATE_CONNECTED) {
            LATTE_LIB_LOG(LOG_ERROR,
                "Error writing to client: %s", connGetLastError(c->conn));
            free_latte_client_async(c);
            return -1;
        }
    }
    if (totwritten > 0) {
        /* For clients representing masters we don't count sending data
         * as an interaction, since we always send REPLCONF ACK commands
         * that take some time to just fill the socket output buffer.
         * We just rely on data / pings received for timeout detection. */
        // if (!(c->flags & CLIENT_MASTER)) c->lastinteraction = server.unixtime;
    }
    if (!clientHasPendingReplies(c)) {
        c->sentlen = 0;
        /* Note that writeToClient() is called in a threaded way, but
         * adDeleteFileEvent() is not thread safe: however writeToClient()
         * is always called with handler_installed set to 0 from threads
         * so we are fine. */
        if (handler_installed) connSetWriteHandler(c->server->el,c->conn, NULL);

        /* Close connection after entire reply has been sent. */
        if (c->flags & CLIENT_CLOSE_AFTER_REPLY) {
            free_latte_client_async(c);
            return -1;
        }
    }
    return 0;
}



