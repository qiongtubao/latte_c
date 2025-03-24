#ifndef __CONNECTION_H
#define __CONNECTION_H

#include <sys/types.h>
#include <ae/ae.h>
#define CONN_INFO_LEN 32

typedef struct connection connection;

#define CONN_FLAG_CLOSE_SCHEDULED   (1<<0)      /* Closed scheduled by a handler */
#define CONN_FLAG_WRITE_BARRIER     (1<<1)      /* Write barrier requested */

#define CONN_TYPE_SOCKET            1
#define CONN_TYPE_TLS               2

#define CONNECTION_OK 0
#define CONNECTION_ERR -1
typedef enum {
    CONN_STATE_NONE = 0,
    CONN_STATE_CONNECTING,
    CONN_STATE_ACCEPTING,
    CONN_STATE_CONNECTED,
    CONN_STATE_CLOSED,
    CONN_STATE_ERROR
} ConnectionState;


/* Anti-warning macro... */
#define UNUSED(V) ((void) V)

typedef void (*ConnectionCallbackFunc)(struct connection *conn);


typedef struct ConnectionType {
    void (*ae_handler)(struct aeEventLoop *el, int fd, void *clientData, int mask);
    int (*connect)(struct aeEventLoop *el, struct connection *conn, const char *addr, int port, const char *source_addr, ConnectionCallbackFunc connect_handler);
    int (*write)(struct connection *conn, const void *data, size_t data_len);
    int (*read)(struct connection *conn, void *buf, size_t buf_len);
    void (*close)(struct aeEventLoop *el, struct connection *conn);
    int (*accept)(struct aeEventLoop *el, struct connection *conn, ConnectionCallbackFunc accept_handler);
    int (*set_write_handler)(struct aeEventLoop *el, struct connection *conn, ConnectionCallbackFunc handler, int barrier);
    int (*set_read_handler)(struct aeEventLoop *el, struct connection *conn, ConnectionCallbackFunc handler);
    const char *(*get_last_error)(struct connection *conn);
    int (*blocking_connect)(struct connection *conn, const char *addr, int port, long long timeout);
    ssize_t (*sync_write)(struct connection *conn, char *ptr, ssize_t size, long long timeout);
    ssize_t (*sync_read)(struct connection *conn, char *ptr, ssize_t size, long long timeout);
    ssize_t (*sync_readline)(struct connection *conn, char *ptr, ssize_t size, long long timeout);
    int (*get_type)(struct connection *conn);
} ConnectionType;

struct connection {
    ConnectionType *type;
    ConnectionState state;
    short int flags;
    short int refs;
    int last_errno;
    void *private_data;
    ConnectionCallbackFunc conn_handler;
    ConnectionCallbackFunc write_handler;
    ConnectionCallbackFunc read_handler;
    int fd;
};

connection *connCreateSocket();
connection *connCreateAcceptedSocket(int fd);
int connGetState(connection *conn);
void *connGetPrivateData(connection *conn);
const char *connGetInfo(connection *conn, char *buf, size_t buf_len);
/* anet-style wrappers to conns */
int connBlock(connection *conn);
int connNonBlock(connection *conn);
int connEnableTcpNoDelay(connection *conn);
void connSetPrivateData(connection *conn, void *data);
int connKeepAlive(connection *conn, int interval);

/* Set a write handler, and possibly enable a write barrier, this flag is
 * cleared when write handler is changed or removed.
 * With barrier enabled, we never fire the event if the read handler already
 * fired in the same event loop iteration. Useful when you want to persist
 * things to disk before sending replies, and want to do that in a group fashion. */
static inline int connSetWriteHandlerWithBarrier(struct aeEventLoop *el, connection *conn, ConnectionCallbackFunc func, int barrier) {
    return conn->type->set_write_handler(el, conn, func, barrier);
}

static inline void connClose(struct aeEventLoop *el, connection *conn) {
    conn->type->close(el, conn);
}

/* Write to connection, behaves the same as write(2).
 *
 * Like write(2), a short write is possible. A -1 return indicates an error.
 *
 * The caller should NOT rely on errno. Testing for an EAGAIN-like condition, use
 * connGetState() to see if the connection state is still CONN_STATE_CONNECTED.
 */
static inline int connWrite(connection *conn, const void *data, size_t data_len) {
    return conn->type->write(conn, data, data_len);
}

/* Returns the last error encountered by the connection, as a string.  If no error,
 * a NULL is returned.
 */
static inline const char *connGetLastError(connection *conn) {
    return conn->type->get_last_error(conn);
}


/* The connection module does not deal with listening and accepting sockets,
 * so we assume we have a socket when an incoming connection is created.
 *
 * The fd supplied should therefore be associated with an already accept()ed
 * socket.
 *
 * connAccept() may directly call accept_handler(), or return and call it
 * at a later time. This behavior is a bit awkward but aims to reduce the need
 * to wait for the next event loop, if no additional handshake is required.
 *
 * IMPORTANT: accept_handler may decide to close the connection, calling connClose().
 * To make this safe, the connection is only marked with CONN_FLAG_CLOSE_SCHEDULED
 * in this case, and connAccept() returns with an error.
 *
 * connAccept() callers must always check the return value and on error (C_ERR)
 * a connClose() must be called.
 */

static inline int connAccept(struct aeEventLoop *el, connection *conn, ConnectionCallbackFunc accept_handler) {
    return conn->type->accept(el, conn, accept_handler);
}

/* Register a read handler, to be called when the connection is readable.
 * If NULL, the existing handler is removed.
 */
static inline int connSetReadHandler(struct aeEventLoop *el, connection *conn, ConnectionCallbackFunc func) {
    return conn->type->set_read_handler(el, conn, func);
}

/* Register a write handler, to be called when the connection is writable.
 * If NULL, the existing handler is removed.
 */
static inline int connSetWriteHandler(struct aeEventLoop *el, connection *conn, ConnectionCallbackFunc func) {
    return conn->type->set_write_handler(el, conn, func, 0);
}


/* 
从连接中读取，与read(2)的行为相同。
与read(2)类似，可能会出现短读。返回值为0表示连接已关闭，返回-1表示出现错误。
调用者不应依赖errno。要测试类似于EAGAIN的条件，请使用connGetState()来查看连接状态是否仍为CONN_STATE_CONNECTED。
 */
static inline int connRead(connection *conn, void *buf, size_t buf_len) {
    return conn->type->read(conn, buf, buf_len);
}

#endif  /* __REDIS_CONNECTION_H */