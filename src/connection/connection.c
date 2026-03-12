#include "connhelpers.h"
#include "zmalloc/zmalloc.h"
#include <sys/socket.h>
#include <errno.h>
#include "anet/anet.h"
#include "utils/utils.h"
#include <sys/time.h>
#include <string.h>
#include "time/localtime.h"

/**
 * @brief 同步 I/O 轮询分辨率（毫秒）
 * 在阻塞 I/O 等待中，每次 ae_wait 的最小等待粒度。
 */
#define SYNCIO__RESOLUTION 10

/**
 * @brief TCP Socket 连接类型实现的前向声明
 * 在文件尾部完整初始化后使用。
 */
ConnectionType CT_Socket;

/**
 * @brief 创建一个新的 TCP Socket 连接（未连接状态）
 * 分配 connection 结构体，绑定 CT_Socket 虚函数表，fd 初始为 -1。
 * @return connection* 新建连接指针
 */
connection *connCreateSocket() {
    connection *conn = zcalloc(sizeof(connection));
    conn->type = &CT_Socket;
    conn->fd = -1;

    return conn;
}

/**
 * @brief 获取 Socket 级别的错误码
 * 通过 getsockopt(SO_ERROR) 获取异步连接错误，失败时返回当前 errno。
 * @param conn 目标连接指针
 * @return int 错误码（0 表示无错误）
 */
int connGetSocketError(connection *conn) {
    int sockerr = 0;
    socklen_t errlen = sizeof(sockerr);

    if (getsockopt(conn->fd, SOL_SOCKET, SO_ERROR, &sockerr, &errlen) == -1)
        sockerr = errno;
    return sockerr;
}

/**
 * @brief Socket 连接的 ae 事件分发处理函数
 *
 * 处理三类事件：
 *   1. 连接建立完成（CONNECTING + WRITABLE）：检查 socket 错误，更新状态，触发 conn_handler
 *   2. 正常读写（考虑写屏障 WRITE_BARRIER）：
 *      - 未启用写屏障：先读后写
 *      - 启用写屏障：先写后读
 * @param el         事件循环指针
 * @param fd         文件描述符
 * @param clientData 连接指针（connection*）
 * @param mask       触发的事件掩码（AE_READABLE / AE_WRITABLE）
 */
static void connSocketEventHandler(struct ae_event_loop_t *el, int fd, void *clientData, int mask)
{
    UNUSED(el);
    UNUSED(fd);
    connection *conn = clientData;

    /* 处理异步连接完成事件 */
    if (conn->state == CONN_STATE_CONNECTING &&
            (mask & AE_WRITABLE) && conn->conn_handler) {

        int conn_error = connGetSocketError(conn);
        if (conn_error) {
            conn->last_errno = conn_error;
            conn->state = CONN_STATE_ERROR;
        } else {
            conn->state = CONN_STATE_CONNECTED;
        }

        /* 无写处理器时删除可写事件监听 */
        if (!conn->write_handler) ae_file_event_delete(el, conn->fd, AE_WRITABLE);

        if (!callHandler(el, conn, conn->conn_handler)) return;
        conn->conn_handler = NULL;
    }

    /* 判断是否启用写屏障（写屏障下先写后读，防止读后立即写） */
    int invert = conn->flags & CONN_FLAG_WRITE_BARRIER;

    int call_write = (mask & AE_WRITABLE) && conn->write_handler;
    int call_read  = (mask & AE_READABLE) && conn->read_handler;

    /* 未启用写屏障：先触发读事件 */
    if (!invert && call_read) {
        if (!callHandler(el, conn, conn->read_handler)) return;
    }
    /* 触发写事件 */
    if (call_write) {
        if (!callHandler(el, conn, conn->write_handler)) return;
    }
    /* 启用写屏障：在写事件后触发读事件 */
    if (invert && call_read) {
        if (!callHandler(el, conn, conn->read_handler)) return;
    }
}

/**
 * @brief 关闭 Socket 连接并释放资源
 *
 * 从事件循环中删除读写监听，关闭 fd。
 * 若在回调内部调用（refs > 0），仅设置 CONN_FLAG_CLOSE_SCHEDULED，
 * 延迟到回调返回后由 callHandler() 执行实际关闭。
 * @param el   事件循环指针
 * @param conn 目标连接指针
 */
static void connSocketClose(struct ae_event_loop_t *el, connection *conn) {
    if (conn->fd != -1) {
        ae_file_event_delete(el, conn->fd, AE_READABLE | AE_WRITABLE);
        close(conn->fd);
        conn->fd = -1;
    }

    /* 在回调内部：标记延迟关闭，不立即释放 */
    if (connHasRefs(conn)) {
        conn->flags |= CONN_FLAG_CLOSE_SCHEDULED;
        return;
    }

    zfree(conn);
}

/**
 * @brief 获取连接的地址信息（本地或远端）
 * @param conn   目标连接指针
 * @param ip     输出：IP 地址字符串
 * @param ip_len IP 缓冲区大小
 * @param port   输出：端口号
 * @param remote 1=获取远端地址，0=获取本地地址
 * @return int CONNECTION_OK 或 CONNECTION_ERR
 */
static int connSocketAddr(connection *conn, char *ip, size_t ip_len, int *port, int remote) {
    if (anetFdToString(conn->fd, ip, ip_len, port, remote) == 0)
        return CONNECTION_OK;

    conn->last_errno = errno;
    return CONNECTION_ERR;
}

/**
 * @brief 向 Socket 写入数据（非阻塞）
 *
 * 写入失败且非 EAGAIN 时，更新 last_errno 并将状态置为 ERROR。
 * @param conn     目标连接指针
 * @param data     待写入数据
 * @param data_len 数据长度
 * @return int 实际写入字节数，-1 表示出错
 */
static int connSocketWrite(connection *conn, const void *data, size_t data_len) {
    int ret = write(conn->fd, data, data_len);
    if (ret < 0 && errno != EAGAIN) {
        conn->last_errno = errno;
        /* 仅在 CONNECTED 状态下更新错误状态，避免覆盖其他状态 */
        if (conn->state == CONN_STATE_CONNECTED)
            conn->state = CONN_STATE_ERROR;
    }

    return ret;
}

/**
 * @brief 从 Socket 读取数据（非阻塞）
 *
 * 读取返回 0 表示连接已关闭；返回 -1 且非 EAGAIN 时置 ERROR 状态。
 * @param conn    目标连接指针
 * @param buf     接收缓冲区
 * @param buf_len 缓冲区大小
 * @return int 实际读取字节数，0=关闭，-1=出错
 */
static int connSocketRead(connection *conn, void *buf, size_t buf_len) {
    int ret = read(conn->fd, buf, buf_len);
    if (!ret) {
        conn->state = CONN_STATE_CLOSED;
    } else if (ret < 0 && errno != EAGAIN) {
        conn->last_errno = errno;
        if (conn->state == CONN_STATE_CONNECTED)
            conn->state = CONN_STATE_ERROR;
    }

    return ret;
}

/**
 * @brief 接受已建立的 Socket 连接并触发 accept_handler
 *
 * 将连接状态从 ACCEPTING 切换到 CONNECTED，然后通过 callHandler 安全触发回调。
 * @param el             事件循环指针
 * @param conn           目标连接指针（必须处于 ACCEPTING 状态）
 * @param accept_handler 连接接受完成后的回调函数
 * @return int CONNECTION_OK 或 CONNECTION_ERR
 */
static int connSocketAccept(struct ae_event_loop_t *el, connection *conn, ConnectionCallbackFunc accept_handler) {
    int ret = CONNECTION_OK;

    if (conn->state != CONN_STATE_ACCEPTING) return CONNECTION_ERR;
    conn->state = CONN_STATE_CONNECTED;

    connIncrRefs(conn); /* 保护连接，防止 accept_handler 内提前释放 */
    if (!callHandler(el, conn, accept_handler)) ret = CONNECTION_ERR;
    connDecrRefs(conn);

    return ret;
}

/**
 * @brief 发起非阻塞 TCP 连接
 *
 * 创建非阻塞 fd，向事件循环注册可写事件以等待连接完成，
 * 并设置 conn_handler 在连接完成时回调。
 * @param el              事件循环指针
 * @param conn            目标连接指针
 * @param addr            目标地址
 * @param port            目标端口
 * @param src_addr        源地址（可为 NULL）
 * @param connect_handler 连接完成回调函数
 * @return int CONNECTION_OK 或 CONNECTION_ERR
 */
static int connSocketConnect(struct ae_event_loop_t *el, connection *conn, const char *addr, int port, const char *src_addr,
        ConnectionCallbackFunc connect_handler) {
    int fd = anetTcpNonBlockBestEffortBindConnect(NULL, addr, port, src_addr);
    if (fd == -1) {
        conn->state = CONN_STATE_ERROR;
        conn->last_errno = errno;
        return CONNECTION_ERR;
    }

    conn->fd = fd;
    conn->state = CONN_STATE_CONNECTING;

    conn->conn_handler = connect_handler;
    /* 注册可写事件：连接完成时触发 connSocketEventHandler */
    ae_file_event_new(el, conn->fd, AE_WRITABLE,
            conn->type->ae_handler, conn);

    return CONNECTION_OK;
}

/**
 * @brief 注册或更新写事件处理器（支持写屏障）
 *
 * 写屏障开启时设置 CONN_FLAG_WRITE_BARRIER，确保写处理器在同一
 * 事件循环迭代中先于读处理器执行。
 * @param el      事件循环指针
 * @param conn    目标连接指针
 * @param func    写事件回调（NULL 表示移除）
 * @param barrier 是否启用写屏障（1=启用）
 * @return int CONNECTION_OK 或 CONNECTION_ERR
 */
static int connSocketSetWriteHandler(struct ae_event_loop_t *el, connection *conn, ConnectionCallbackFunc func, int barrier) {
    if (func == conn->write_handler) return CONNECTION_OK;

    conn->write_handler = func;
    if (barrier)
        conn->flags |= CONN_FLAG_WRITE_BARRIER;
    else
        conn->flags &= ~CONN_FLAG_WRITE_BARRIER;
    if (!conn->write_handler)
        ae_file_event_delete(el, conn->fd, AE_WRITABLE);
    else
        if (ae_file_event_new(el, conn->fd, AE_WRITABLE,
                    conn->type->ae_handler, conn) == AE_ERR) return CONNECTION_ERR;
    return CONNECTION_OK;
}

/**
 * @brief 注册或移除读事件处理器
 * @param el   事件循环指针
 * @param conn 目标连接指针
 * @param func 读事件回调（NULL 表示移除）
 * @return int CONNECTION_OK 或 CONNECTION_ERR
 */
static int connSocketSetReadHandler(struct ae_event_loop_t *el, connection *conn, ConnectionCallbackFunc func) {
    if (func == conn->read_handler) return CONNECTION_OK;

    conn->read_handler = func;
    if (!conn->read_handler)
        ae_file_event_delete(el, conn->fd, AE_READABLE);
    else
        if (ae_file_event_new(el, conn->fd,
                    AE_READABLE, conn->type->ae_handler, conn) == AE_ERR) return CONNECTION_ERR;
    return CONNECTION_OK;
}

/**
 * @brief 返回最近一次错误的字符串描述
 * @param conn 目标连接指针
 * @return const char* 错误字符串
 */
static const char *connSocketGetLastError(connection *conn) {
    return strerror(conn->last_errno);
}

/**
 * @brief 阻塞式 TCP 连接（带超时）
 *
 * 创建非阻塞 fd，通过 ae_wait 等待可写事件（超时后返回错误）。
 * @param conn    目标连接指针
 * @param addr    目标地址
 * @param port    目标端口
 * @param timeout 超时时间（毫秒）
 * @return int CONNECTION_OK 或 CONNECTION_ERR
 */
static int connSocketBlockingConnect(connection *conn, const char *addr, int port, long long timeout) {
    int fd = anetTcpNonBlockConnect(NULL, addr, port);
    if (fd == -1) {
        conn->state = CONN_STATE_ERROR;
        conn->last_errno = errno;
        return CONNECTION_ERR;
    }

    if ((ae_wait(fd, AE_WRITABLE, timeout) & AE_WRITABLE) == 0) {
        conn->state = CONN_STATE_ERROR;
        conn->last_errno = ETIMEDOUT;
    }

    conn->fd = fd;
    conn->state = CONN_STATE_CONNECTED;
    return CONNECTION_OK;
}

/**
 * @brief 同步写入指定字节数到 fd（带超时，毫秒）
 *
 * 乐观先写再等待，循环直至写完 size 字节或超时。
 * 超时返回 -1 并设置 errno=ETIMEDOUT。
 * @param fd      文件描述符
 * @param ptr     待写入数据
 * @param size    待写入字节数
 * @param timeout 超时时间（毫秒）
 * @return ssize_t 成功返回 size，失败返回 -1
 */
ssize_t syncWrite(int fd, char *ptr, ssize_t size, long long timeout) {
    ssize_t nwritten, ret = size;
    long long start = mstime();
    long long remaining = timeout;

    while(1) {
        long long wait = (remaining > SYNCIO__RESOLUTION) ?
                          remaining : SYNCIO__RESOLUTION;
        long long elapsed;

        /* 乐观尝试写入，最坏情况得到 EAGAIN */
        nwritten = write(fd, ptr, size);
        if (nwritten == -1) {
            if (errno != EAGAIN) return -1;
        } else {
            ptr += nwritten;
            size -= nwritten;
        }
        if (size == 0) return ret; /* 全部写完 */

        /* 等待可写事件 */
        ae_wait(fd, AE_WRITABLE, wait);
        elapsed = mstime() - start;
        if (elapsed >= timeout) {
            errno = ETIMEDOUT;
            return -1;
        }
        remaining = timeout - elapsed;
    }
}

/**
 * @brief connection 封装的同步写入
 * @param conn    目标连接指针
 * @param ptr     待写入数据
 * @param size    待写入字节数
 * @param timeout 超时时间（毫秒）
 * @return ssize_t 实际写入字节数
 */
static ssize_t connSocketSyncWrite(connection *conn, char *ptr, ssize_t size, long long timeout) {
    return syncWrite(conn->fd, ptr, size, timeout);
}

/**
 * @brief 同步读取指定字节数从 fd（带超时，毫秒）
 *
 * 乐观先读再等待，循环直至读完 size 字节或超时。
 * 读到 0 字节（连接关闭）返回 -1；超时返回 -1 并设置 errno=ETIMEDOUT。
 * @param fd      文件描述符
 * @param ptr     接收缓冲区
 * @param size    待读取字节数
 * @param timeout 超时时间（毫秒）
 * @return ssize_t 成功返回 size，失败返回 -1
 */
ssize_t syncRead(int fd, char *ptr, ssize_t size, long long timeout) {
    ssize_t nread, totread = 0;
    long long start = mstime();
    long long remaining = timeout;

    if (size == 0) return 0;
    while(1) {
        long long wait = (remaining > SYNCIO__RESOLUTION) ?
                          remaining : SYNCIO__RESOLUTION;
        long long elapsed;

        /* 乐观尝试读取 */
        nread = read(fd, ptr, size);
        if (nread == 0) return -1; /* 短读：连接已关闭 */
        if (nread == -1) {
            if (errno != EAGAIN) return -1;
        } else {
            ptr += nread;
            size -= nread;
            totread += nread;
        }
        if (size == 0) return totread; /* 全部读完 */

        /* 等待可读事件 */
        ae_wait(fd, AE_READABLE, wait);
        elapsed = mstime() - start;
        if (elapsed >= timeout) {
            errno = ETIMEDOUT;
            return -1;
        }
        remaining = timeout - elapsed;
    }
}

/**
 * @brief connection 封装的同步读取
 * @param conn    目标连接指针
 * @param ptr     接收缓冲区
 * @param size    待读取字节数
 * @param timeout 超时时间（毫秒）
 * @return ssize_t 实际读取字节数
 */
static ssize_t connSocketSyncRead(connection *conn, char *ptr, ssize_t size, long long timeout) {
    return syncRead(conn->fd, ptr, size, timeout);
}

/**
 * @brief 同步读取一行（以 '\\n' 结尾）从 fd（带超时）
 *
 * 逐字节读取，遇到 '\\n' 时停止，结果以 '\\0' 结尾，并去除末尾 '\\r'。
 * @param fd      文件描述符
 * @param ptr     接收缓冲区
 * @param size    缓冲区大小（含 '\\0'）
 * @param timeout 超时时间（毫秒）
 * @return ssize_t 读取的字节数（不含 '\\0'），失败返回 -1
 */
ssize_t syncReadLine(int fd, char *ptr, ssize_t size, long long timeout) {
    ssize_t nread = 0;

    size--; /* 预留 '\0' 位置 */
    while(size) {
        char c;

        if (syncRead(fd, &c, 1, timeout) == -1) return -1;
        if (c == '\n') {
            *ptr = '\0';
            if (nread && *(ptr-1) == '\r') *(ptr-1) = '\0'; /* 去除 \r */
            return nread;
        } else {
            *ptr++ = c;
            *ptr = '\0';
            nread++;
        }
        size--;
    }
    return nread;
}

/**
 * @brief connection 封装的同步行读取
 * @param conn    目标连接指针
 * @param ptr     接收缓冲区
 * @param size    缓冲区大小
 * @param timeout 超时时间（毫秒）
 * @return ssize_t 读取字节数，失败返回 -1
 */
static ssize_t connSocketSyncReadLine(connection *conn, char *ptr, ssize_t size, long long timeout) {
    return syncReadLine(conn->fd, ptr, size, timeout);
}

/**
 * @brief 返回连接类型标识（CONN_TYPE_SOCKET）
 * @param conn 目标连接指针（未使用）
 * @return int CONN_TYPE_SOCKET
 */
static int connSocketGetType(connection *conn) {
    (void) conn;

    return CONN_TYPE_SOCKET;
}

/** @brief TCP Socket 连接虚函数表实例 */
ConnectionType CT_Socket = {
    .ae_handler = connSocketEventHandler,
    .addr = connSocketAddr,
    .close = connSocketClose,
    .write = connSocketWrite,
    .read = connSocketRead,
    .accept = connSocketAccept,
    .connect = connSocketConnect,
    .set_write_handler = connSocketSetWriteHandler,
    .set_read_handler = connSocketSetReadHandler,
    .get_last_error = connSocketGetLastError,
    .blocking_connect = connSocketBlockingConnect,
    .sync_write = connSocketSyncWrite,
    .sync_read = connSocketSyncRead,
    .sync_readline = connSocketSyncReadLine,
    .get_type = connSocketGetType
};

/**
 * @brief 从已 accept() 的 fd 创建连接对象
 *
 * 连接初始处于 ACCEPTING 状态，需调用 connAccept() 并触发
 * accept_handler 后才能进行 I/O。
 * @param fd 已接受的套接字文件描述符
 * @return connection* 新建连接指针
 */
connection *connCreateAcceptedSocket(int fd) {
    connection *conn = connCreateSocket();
    conn->fd = fd;
    conn->state = CONN_STATE_ACCEPTING;
    return conn;
}

/**
 * @brief 获取连接当前状态
 * @param conn 目标连接指针
 * @return int ConnectionState 枚举值
 */
int conn_get_state(connection *conn) {
    return conn->state;
}

/**
 * @brief 格式化连接信息为字符串（"fd=<fdnum>"）
 * @param conn    目标连接指针（NULL 时返回 "fd=-1"）
 * @param buf     输出缓冲区
 * @param buf_len 缓冲区大小
 * @return const char* 指向 buf 的指针
 */
const char *connGetInfo(connection *conn, char *buf, size_t buf_len) {
    snprintf(buf, buf_len-1, "fd=%i", conn == NULL ? -1 : conn->fd);
    return buf;
}

/**
 * @brief 获取连接的私有数据指针
 * @param conn 目标连接指针
 * @return void* 私有数据指针
 */
void *connGetPrivateData(connection *conn) {
    return conn->private_data;
}

/**
 * @brief 将连接设为阻塞模式
 * @param conn 目标连接指针
 * @return int 成功返回 0，fd 无效返回 CONNECTION_ERR
 */
int connBlock(connection *conn) {
    if (conn->fd == -1) return CONNECTION_ERR;
    return anetBlock(NULL, conn->fd);
}

/**
 * @brief 将连接设为非阻塞模式
 * @param conn 目标连接指针
 * @return int 成功返回 0，fd 无效返回 CONNECTION_ERR
 */
int connNonBlock(connection *conn) {
    if (conn->fd == -1) return CONNECTION_ERR;
    return anetNonBlock(NULL, conn->fd);
}

/**
 * @brief 启用 TCP_NODELAY（禁用 Nagle 算法，降低小包延迟）
 * @param conn 目标连接指针
 * @return int 成功返回 0，fd 无效返回 CONNECTION_ERR
 */
int connEnableTcpNoDelay(connection *conn) {
    if (conn->fd == -1) return CONNECTION_ERR;
    return anetEnableTcpNoDelay(NULL, conn->fd);
}

/**
 * @brief 设置连接的私有数据指针
 * @param conn 目标连接指针
 * @param data 私有数据指针
 */
void connSetPrivateData(connection *conn, void *data) {
    conn->private_data = data;
}

/**
 * @brief 启用 TCP KeepAlive 并设置探测间隔
 * @param conn     目标连接指针
 * @param interval KeepAlive 探测间隔（秒）
 * @return int 成功返回 0，fd 无效返回 CONNECTION_ERR
 */
int connKeepAlive(connection *conn, int interval) {
    if (conn->fd == -1) return CONNECTION_ERR;
    return anetKeepAlive(NULL, conn->fd, interval);
}
