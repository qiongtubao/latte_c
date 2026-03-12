#include "client.h"
#include <assert.h>
#include <string.h>
// #ifdef USE_ASYNC_IO
#include "async_io/async_io.h"
#include "utils/utils.h"
// #endif
sds_t latte_client_info_to_sds(latte_client_t* client);


/**
 * @brief 异步 IO 写请求完成回调
 *
 * 当异步 IO 写操作完成时由 async_io 层调用：
 *   1. 从 clients_async_pending_write 链表中移除此客户端节点
 *   2. 记录结束时间并调用 client->end 回调（若存在）
 *   3. 重置 request 的 len 和 is_finished 状态，以便下次复用
 *
 * @param request 已完成的异步 IO 请求指针，ctx 字段指向 latte_client_t
 */
void client_async_io_write_finished(async_io_request_t* request) {
    latte_client_t* client = request->ctx;
    list_del_node(client->server->clients_async_pending_write, client->async_io_client_node);
    client->async_io_client_node = NULL;
    
    //这里只处理 一次async_io 情况  后期想想如何处理多次async_io
    client->end_time = ustime();
    if (client->end != NULL) {
        client->end(client);
    }

    // reset
    request->len = -1;
    request->is_finished = false;
    LATTE_LIB_LOG(LOG_DEBUG, "client_async_io_write_finished %d", request->len);
}

/**
 * @brief 初始化客户端的受保护字段（在 createClient 后、connAccept 前调用）
 *
 * 清零/初始化：查询缓冲区、回复链表、输出缓冲位置、发送偏移、
 * 异步 IO 缓存请求（预分配 CLIENT_ASYNC_IO_MAX_SIZE 字节缓冲区）。
 * @param client 目标客户端指针
 */
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
    client->name = NULL;
    client->peer_id = NULL;
    
}


/**
 * @brief 连接接受完成回调（connAccept 异步完成后调用）
 * 检查连接是否成功（CONN_STATE_CONNECTED），失败时异步释放客户端。
 * @param conn 已完成握手的连接对象
 */
void clientAcceptHandler(connection *conn) {
    latte_client_t *c = connGetPrivateData(conn);

    if (conn_get_state(conn) != CONN_STATE_CONNECTED) {
        LATTE_LIB_LOG(LOG_ERROR, "Error accepting a client connection: %s\n", conn_get_last_error(conn));
        free_latte_client_async(c);
        return;
    }
}

/**
 * @brief 完成客户端初始化：合并 flags 并调用 connAccept 触发握手
 *
 * connAccept 可能同步（立刻调用 clientAcceptHandler）或
 * 异步（调度后调用），因此本函数返回后不可再访问 conn。
 * 握手失败时直接释放客户端。
 *
 * @param el    事件循环指针
 * @param c     目标客户端指针
 * @param conn  底层连接对象
 * @param flags 追加到 c->flags 的初始标志
 */
void init_latte_client(struct ae_event_loop_t* el, latte_client_t* c, struct connection* conn, int flags) {
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
        if (conn_get_state(conn) == CONN_STATE_ERROR) {
            LATTE_LIB_LOG(LOG_ERROR, "Error accepting a client connection: %s (conn: %s)\n", 
                conn_get_last_error(conn), 
                connGetInfo(conn, conninfo, sizeof(conninfo))
            );
        }
        free_latte_client(connGetPrivateData(conn));
        return;
    }
}

/**
 * @brief 销毁客户端内部资源（释放 querybuf）
 * 不释放 client 指针本身，由上层 freeClient 负责。
 * @param client 目标客户端指针
 */
void destory_latte_client(latte_client_t* client) {
    if (client->querybuf != NULL) {
        sds_delete(client->querybuf);
        client->querybuf = NULL;
    }
}



/**
 * @brief 将客户端链接到服务器的客户端列表和 ID 索引
 *
 * 追加到 server->clients 尾部，记录链表节点指针（用于 O(1) 移除），
 * 同时以大端序 ID 为键插入 clients_index 基数树。
 * @param server 目标服务器指针
 * @param c      要链接的客户端指针
 */
void link_latte_client(latte_server_t* server, latte_client_t* c) {
    LATTE_LIB_LOG(LOG_DEBUG,"link_latte_client %d", c->conn->fd);
    list_add_node_tail(server->clients, c);
    /* 请注意，我们记住了存储客户端的链表节点，这样在 unlinkClient() 中移除客户端将不需要进行线性扫描，而只需要进行一个常数时间的操作。 */
    c->client_list_node = list_last(server->clients);
    uint64_t id = htonu64(c->id);
    raxInsert(server->clients_index,(unsigned char*)&id,sizeof(id),c,NULL);
}
/**
 * @brief 从服务器全局列表和索引中移除客户端并关闭连接
 *
 * 从 server->clients 链表移除（O(1)），从 clients_index 基数树移除，
 * 然后关闭底层连接。不释放 client 内存，由 free_latte_client 负责。
 * @param c 目标客户端指针
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

/**
 * @brief 同步释放客户端：unlink → 销毁内部资源 → 调用 server->freeClient
 * @param c 目标客户端指针
 */
void free_latte_client(latte_client_t *c) {
    LATTE_LIB_LOG(LOG_DEBUG,"freeClient %d\n", c->conn->fd);
    latte_server_t* server = c->server;
    unlink_latte_client(c);
    destory_latte_client(c);
    server->freeClient(c);
}

/**
 * @brief 异步释放客户端（当前实现直接同步释放，TODO：实现真正的异步延迟释放）
 * @param c 目标客户端指针
 */
void free_latte_client_async(latte_client_t *c) {
    struct latte_server_t* server = c->server;
    //TODO
    free_latte_client(c);
}

/**
 * @brief 连接可读事件回调：从 socket 读取数据并执行命令
 *
 * 执行流程：
 *   1. 记录 start_time，调用 start 回调
 *   2. 扩容 querybuf，调用 conn_read 读取数据
 *   3. 读取失败/连接断开时异步释放客户端
 *   4. 调用 exec 回调解析并执行命令，完成后重置 querybuf
 *   5. 尝试异步 IO 写回复，记录 exec_end_time
 *
 * @param conn 触发可读事件的连接对象，私有数据为 latte_client_t*
 */
void read_query_from_client(connection *conn) { 
    latte_client_t *c = connGetPrivateData(conn);
    c->start_time = ustime();
    if (c->start) c->start(c);
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
    c->read_time = ustime();
    nread = conn_read(c->conn, c->querybuf + qblen, readlen);
    if (nread == -1) {
        if (conn_get_state(conn) == CONN_STATE_CONNECTED) {
            return;
        } else {
            LATTE_LIB_LOG(LOG_ERROR, "Reading from client: %s\n",conn_get_last_error(c->conn));
            free_latte_client_async(c);
            return;
        }
    } else if (nread == 0) {
        free_latte_client_async(c);
        return;
    } 
    sds_incr_len(c->querybuf,nread);
    LATTE_LIB_LOG(LOG_DEBUG, "read_query_from_client exec %s %d", c->querybuf, nread);
    c->exec_time = ustime();
    if (c->exec(c, nread)) {
        //清理掉c->querybuf
        sds_range(c->querybuf,c->qb_pos,-1);
        c->qb_pos = 0;
    }
    c->exec_end_time = ustime();
    if (async_io_try_write(c)) {
        /* 如果使用异步io 则记录写入时间 */
        c->write_time = ustime(); // | ASYNC_IO_WRITE_TIME_FLAG
    }
}


/**
 * @brief 尝试将回复数据写入固定输出缓冲区 buf[]
 *
 * 条件：CLIENT_CLOSE_AFTER_REPLY 标志未设置、reply 链表为空、
 * buf[] 剩余空间足够。满足则 memcpy 并更新 bufpos。
 * @param c   目标客户端指针
 * @param s   回复数据指针
 * @param len 回复数据长度
 * @return int 成功写入返回 0，条件不满足返回 -1
 */
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

/**
 * @brief 将回复数据追加到 reply 链表（固定缓冲区溢出时使用）
 *
 * 优先填满链表尾部块的剩余空间，若仍有剩余则分配新的
 * client_reply_block_t（至少 PROTO_REPLY_CHUNK_BYTES 大小），
 * 并更新 reply_bytes 统计。
 * @param c   目标客户端指针
 * @param s   回复数据指针
 * @param len 回复数据长度
 */
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


/**
 * @brief 尝试通过异步 IO 发送缓存的回复数据
 *
 * 若 async_io_request_cache->len > 0，提交异步 IO 写请求：
 *   - 成功：客户端加入 clients_async_pending_write 队列，返回 1
 *   - 失败：降级到同步路径，数据写入 clients_pending_write，返回 0
 * @param c 目标客户端指针
 * @return int 异步 IO 提交成功返回 1，否则返回 0
 */
int async_io_try_write(latte_client_t* c) {
    if (c->async_io_request_cache->len > 0) {
        if (async_io_net_write(c->async_io_request_cache)) {
            list_add_node_tail(c->server->clients_async_pending_write, c);
            c->async_io_client_node = list_last(c->server->clients_async_pending_write);
            return 1;
        } else {
            list_add_node_tail(c->server->clients_pending_write, c);
            if (_add_reply_to_buffer(c,c->async_io_request_cache->buf,c->async_io_request_cache->len) != 0)
                _add_reply_proto_to_list(c,c->async_io_request_cache->buf,c->async_io_request_cache->len);
            c->async_io_request_cache->len = -1;
            return 0;
        }
        
    }
    return 0;
}

/**
 * @brief 向客户端追加回复数据（自动选择异步 IO / 同步缓冲路径）
 *
 * 若服务器启用异步 IO 或已有异步 IO 缓存数据，且总长度未超过
 * CLIENT_ASYNC_IO_MAX_SIZE，则追加到 async_io_request_cache；
 * 否则写入 clients_pending_write 队列，依次尝试固定缓冲区和链表。
 * @param c   目标客户端指针
 * @param s   回复数据指针
 * @param len 回复数据长度
 */
#define max(a, b) ((a) > (b) ? (a) : (b))
void add_reply_proto(latte_client_t* c, const char* s, size_t len) {
    //TODO use async_io 
    // #ifdef USE_ASYNC_IO
    if (c->server->use_async_io || c->async_io_request_cache->len > 0) {
        if (max(c->async_io_request_cache->len, 0) + len < CLIENT_ASYNC_IO_MAX_SIZE) {
            LATTE_LIB_LOG(LOG_DEBUG, "add_reply_proto use async_io %d", len);
            latte_assert_with_info(c->async_io_request_cache->len == -1, "async_io_request_cache->len != -1");
            if (c->async_io_request_cache->len == -1) {
                c->async_io_request_cache->len = 0;
            }
            memcpy(c->async_io_request_cache->buf + c->async_io_request_cache->len, s, len); 
            c->async_io_request_cache->len += len;
            c->async_io_request_cache->fd = c->conn->fd; 
            LATTE_LIB_LOG(LOG_DEBUG, "add_reply_proto use async_io %d", c->async_io_request_cache->len);
            return;
        } else {
            //async_io 发送大量数据 如何处理  暂时还没想好如何处理
            latte_assert_with_info(0, "async_io_request_cache->len + len > CLIENT_ASYNC_IO_MAX_SIZE");
            c->async_io_request_cache->len = -1;
        }
    }
    // #endif
    LATTE_LIB_LOG(LOG_DEBUG, "add_reply_proto use buffer %d", len);
    list_add_node_tail(c->server->clients_pending_write, c);
    if (_add_reply_to_buffer(c,s,len) != 0)
        _add_reply_proto_to_list(c,s,len);
}


/**
 * @brief 检查客户端是否有待发送的回复数据
 * @param c 目标客户端指针
 * @return int 有待发数据（bufpos > 0 或 reply 非空）返回非 0，否则返回 0
 */
int client_has_pending_replies(latte_client_t *c) {
    return c->bufpos || list_length(c->reply);
}
/**
 * @brief 将客户端输出缓冲区数据写入连接 socket
 *
 * 先发送固定缓冲区 buf[]，再发送 reply 链表中的块。
 * 每个块发送完毕后从链表移除。发送完所有数据后若设置了
 * CLIENT_CLOSE_AFTER_REPLY 则异步关闭客户端。
 * 此函数可被 IO 线程调用（handler_installed=0 时线程安全）。
 *
 * @param c                 目标客户端指针
 * @param handler_installed 非 0 时写完后卸载写事件处理器
 * @return int 正常返回 0，连接断开或出错返回 -1
 */
int write_to_client(latte_client_t *c, int handler_installed) {
    // LATTE_LIB_LOG(LOG_DEBUG, "write_to_client %d", c->conn->fd);
    /* Update total number of writes on server */
    // atomicIncr(c->server.stat_total_writes_processed, 1);
    
    c->write_time = ustime();
    ssize_t nwritten = 0, totwritten = 0;
    size_t objlen;
    client_reply_block_t *o;
    while(client_has_pending_replies(c)) {
        if (c->bufpos > 0) {
            nwritten = conn_write(c->conn,c->buf+c->sentlen,c->bufpos-c->sentlen);
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
           
            nwritten = conn_write(c->conn, o->buf + c->sentlen, objlen - c->sentlen);
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
        if (conn_get_state(c->conn) != CONN_STATE_CONNECTED) {
            LATTE_LIB_LOG(LOG_ERROR,
                "Error writing to client: %s", conn_get_last_error(c->conn));
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
    if (!client_has_pending_replies(c)) {
        c->sentlen = 0;
        /* Note that write_to_client() is called in a threaded way, but
         * adDeleteFileEvent() is not thread safe: however write_to_client()
         * is always called with handler_installed set to 0 from threads
         * so we are fine. */
        if (handler_installed) conn_set_write_handler(c->server->el,c->conn, NULL);

        /* Close connection after entire reply has been sent. */
        if (c->flags & CLIENT_CLOSE_AFTER_REPLY) {
            free_latte_client_async(c);
            return -1;
        }
    }
    c->end_time = ustime();
    if (c->end) c->end(c);
    return 0;
}



/**
 * @brief 获取客户端名称字符串
 * @param c 目标客户端指针
 * @return sds 客户端名称（可能为 NULL）
 */
sds client_get_name(latte_client_t* c) {
    return c->name;
}
#define NET_IP_STR_LEN 46 /* INET6_ADDRSTRLEN is 46, but we need to be sure */
#define NET_ADDR_STR_LEN (NET_IP_STR_LEN+32) /* Must be enough for ip:port */

void gen_client_addr_string(latte_client_t* c, char* addr, size_t addr_len, int remote) {
    // if (c->flags & CLIENT_UNIX_SOCKET) {
    //     /* Unix socket client. */
    //     snprintf(addr,addr_len,"%s:0",server.unixsocket);
    // } else {
        /* TCP client. */
        conn_format_addr(c->conn,addr, addr_len,remote);
    // }
}

/**
 * @brief 获取客户端对端地址字符串（懒加载）
 * 首次调用时通过 conn_format_addr 生成 "ip:port" 格式的 SDS 字符串，
 * 后续调用直接返回缓存结果。
 * @param c 目标客户端指针
 * @return sds 对端地址字符串
 */
sds client_get_peer_id(latte_client_t* c) {
    if (c->peer_id == NULL) {
        char peer_id[NET_ADDR_STR_LEN] = {0};
        gen_client_addr_string(c, peer_id, sizeof(peer_id), 1);
        c->peer_id = sds_new(peer_id);
    }
    return c->peer_id;
}