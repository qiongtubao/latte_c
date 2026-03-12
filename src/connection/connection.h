#ifndef __CONNECTION_H
#define __CONNECTION_H

#include <sys/types.h>
#include <ae/ae.h>
#include <string.h>

/** @brief 连接信息字符串最大长度 */
#define CONN_INFO_LEN 32

typedef struct connection connection;

/** @brief 连接关闭已被调度（标志位 0） */
#define CONN_FLAG_CLOSE_SCHEDULED   (1<<0)
/** @brief 已请求写屏障（标志位 1） */
#define CONN_FLAG_WRITE_BARRIER     (1<<1)

/** @brief 连接类型：普通 TCP Socket */
#define CONN_TYPE_SOCKET            1
/** @brief 连接类型：TLS 加密连接 */
#define CONN_TYPE_TLS               2

/** @brief 连接操作成功返回值 */
#define CONNECTION_OK 0
/** @brief 连接操作失败返回值 */
#define CONNECTION_ERR -1

/**
 * @brief 连接状态枚举
 */
typedef enum {
    CONN_STATE_NONE = 0,    /**< 初始状态，未使用 */
    CONN_STATE_CONNECTING,  /**< 正在连接中 */
    CONN_STATE_ACCEPTING,   /**< 正在等待接受 */
    CONN_STATE_CONNECTED,   /**< 已建立连接 */
    CONN_STATE_CLOSED,      /**< 连接已关闭 */
    CONN_STATE_ERROR        /**< 连接出错 */
} ConnectionState;

/** @brief 消除未使用变量警告的宏 */
#define UNUSED(V) ((void) V)

/**
 * @brief 连接事件回调函数类型
 * @param conn 触发事件的连接指针
 */
typedef void (*ConnectionCallbackFunc)(struct connection *conn);

/**
 * @brief 连接操作虚函数表
 * 通过函数指针实现 Socket/TLS 等不同连接类型的统一操作接口。
 */
typedef struct ConnectionType {
    /** @brief ae 事件循环分发处理函数 */
    void (*ae_handler)(struct ae_event_loop_t *el, int fd, void *clientData, int mask);
    /** @brief 发起异步连接 */
    int (*connect)(struct ae_event_loop_t *el, struct connection *conn, const char *addr, int port, const char *source_addr, ConnectionCallbackFunc connect_handler);
    /** @brief 获取连接的本地或远端地址 */
    int (*addr)(connection *conn, char *ip, size_t ip_len, int *port, int remote);
    /** @brief 向连接写入数据 */
    int (*write)(struct connection *conn, const void *data, size_t data_len);
    /** @brief 从连接读取数据 */
    int (*read)(struct connection *conn, void *buf, size_t buf_len);
    /** @brief 关闭并释放连接 */
    void (*close)(struct ae_event_loop_t *el, struct connection *conn);
    /** @brief 接受新连接并设置回调 */
    int (*accept)(struct ae_event_loop_t *el, struct connection *conn, ConnectionCallbackFunc accept_handler);
    /** @brief 注册写事件处理器（支持写屏障） */
    int (*set_write_handler)(struct ae_event_loop_t *el, struct connection *conn, ConnectionCallbackFunc handler, int barrier);
    /** @brief 注册读事件处理器 */
    int (*set_read_handler)(struct ae_event_loop_t *el, struct connection *conn, ConnectionCallbackFunc handler);
    /** @brief 获取最近一次错误的描述字符串 */
    const char *(*get_last_error)(struct connection *conn);
    /** @brief 阻塞式连接（带超时，毫秒） */
    int (*blocking_connect)(struct connection *conn, const char *addr, int port, long long timeout);
    /** @brief 同步写入数据（带超时） */
    ssize_t (*sync_write)(struct connection *conn, char *ptr, ssize_t size, long long timeout);
    /** @brief 同步读取数据（带超时） */
    ssize_t (*sync_read)(struct connection *conn, char *ptr, ssize_t size, long long timeout);
    /** @brief 同步读取一行数据（带超时） */
    ssize_t (*sync_readline)(struct connection *conn, char *ptr, ssize_t size, long long timeout);
    /** @brief 获取连接类型标识（CONN_TYPE_*） */
    int (*get_type)(struct connection *conn);
} ConnectionType;

/**
 * @brief 连接结构体
 * 封装连接的类型、状态、标志及事件回调，由 ConnectionType 虚函数表驱动。
 */
struct connection {
    ConnectionType *type;               /**< 虚函数表指针（决定 Socket/TLS 行为） */
    ConnectionState state;              /**< 当前连接状态 */
    short int flags;                    /**< 连接标志位（CONN_FLAG_*） */
    short int refs;                     /**< 引用计数（防止在回调中提前释放） */
    int last_errno;                     /**< 最近一次错误码（errno） */
    void *private_data;                 /**< 上层私有数据指针（如 client 结构体） */
    ConnectionCallbackFunc conn_handler;  /**< 连接建立/接受完成回调 */
    ConnectionCallbackFunc write_handler; /**< 可写事件回调 */
    ConnectionCallbackFunc read_handler;  /**< 可读事件回调 */
    int fd;                             /**< 底层文件描述符 */
};

/**
 * @brief 创建一个新的 TCP Socket 连接（未连接状态）
 * @return connection* 新建连接指针
 */
connection *connCreateSocket();

/**
 * @brief 从已 accept() 的 fd 创建连接对象（ACCEPTING 状态）
 * @param fd 已接受的套接字文件描述符
 * @return connection* 新建连接指针
 */
connection *connCreateAcceptedSocket(int fd);

/**
 * @brief 获取连接当前状态
 * @param conn 目标连接指针
 * @return int ConnectionState 枚举值
 */
int conn_get_state(connection *conn);

/**
 * @brief 获取连接的私有数据指针
 * @param conn 目标连接指针
 * @return void* 私有数据指针
 */
void *connGetPrivateData(connection *conn);

/**
 * @brief 格式化连接信息为字符串（类型+状态+fd）
 * @param conn    目标连接指针
 * @param buf     输出缓冲区
 * @param buf_len 缓冲区大小
 * @return const char* 指向 buf 的指针
 */
const char *connGetInfo(connection *conn, char *buf, size_t buf_len);

/**
 * @brief 将连接设为阻塞模式
 * @param conn 目标连接指针
 * @return int 成功返回 0，失败返回 -1
 */
int connBlock(connection *conn);

/**
 * @brief 将连接设为非阻塞模式
 * @param conn 目标连接指针
 * @return int 成功返回 0，失败返回 -1
 */
int connNonBlock(connection *conn);

/**
 * @brief 启用 TCP_NODELAY（禁用 Nagle 算法）
 * @param conn 目标连接指针
 * @return int 成功返回 0，失败返回 -1
 */
int connEnableTcpNoDelay(connection *conn);

/**
 * @brief 设置连接的私有数据指针
 * @param conn 目标连接指针
 * @param data 私有数据指针
 */
void connSetPrivateData(connection *conn, void *data);

/**
 * @brief 启用 TCP KeepAlive 并设置探测间隔
 * @param conn     目标连接指针
 * @param interval KeepAlive 探测间隔（秒）
 * @return int 成功返回 0，失败返回 -1
 */
int connKeepAlive(connection *conn, int interval);

/**
 * @brief 注册写事件处理器（可选写屏障）
 *
 * 写屏障开启时，若当前事件循环迭代中读事件已触发，则跳过写事件，
 * 适用于需要先落盘再回复的场景。
 * @param el      事件循环指针
 * @param conn    目标连接指针
 * @param func    写事件回调函数（NULL 表示移除）
 * @param barrier 是否启用写屏障（1=启用，0=不启用）
 * @return int 成功返回 0，失败返回 -1
 */
static inline int conn_set_write_handlerWithBarrier(struct ae_event_loop_t *el, connection *conn, ConnectionCallbackFunc func, int barrier) {
    return conn->type->set_write_handler(el, conn, func, barrier);
}

/**
 * @brief 关闭连接并释放资源
 * @param el   事件循环指针
 * @param conn 目标连接指针
 */
static inline void connClose(struct ae_event_loop_t *el, connection *conn) {
    conn->type->close(el, conn);
}

/**
 * @brief 向连接写入数据（行为同 write(2)）
 *
 * 可能发生短写；返回 -1 表示出错。
 * 调用方不应依赖 errno，应通过 conn_get_state() 检查连接状态。
 * @param conn     目标连接指针
 * @param data     待写入数据
 * @param data_len 数据长度
 * @return int 实际写入字节数，-1 表示出错
 */
static inline int conn_write(connection *conn, const void *data, size_t data_len) {
    return conn->type->write(conn, data, data_len);
}

/**
 * @brief 获取最近一次连接错误的描述字符串
 * @param conn 目标连接指针
 * @return const char* 错误字符串，无错误时返回 NULL
 */
static inline const char *conn_get_last_error(connection *conn) {
    return conn->type->get_last_error(conn);
}

/**
 * @brief 接受新连接并触发回调
 *
 * 连接模块不处理监听和 accept()，调用此函数前 fd 已完成 accept()。
 * accept_handler 可能立即被调用，也可能在下一次事件循环触发。
 * 若 accept_handler 内部调用了 connClose()，连接仅设置
 * CONN_FLAG_CLOSE_SCHEDULED，connAccept() 返回错误，调用方必须
 * 检查返回值并在出错时调用 connClose()。
 * @param el             事件循环指针
 * @param conn           目标连接指针
 * @param accept_handler 接受完成后的回调函数
 * @return int 成功返回 CONNECTION_OK，失败返回 CONNECTION_ERR
 */
static inline int connAccept(struct ae_event_loop_t *el, connection *conn, ConnectionCallbackFunc accept_handler) {
    return conn->type->accept(el, conn, accept_handler);
}

/**
 * @brief 注册读事件处理器
 * @param el   事件循环指针
 * @param conn 目标连接指针
 * @param func 读事件回调（NULL 表示移除）
 * @return int 成功返回 0，失败返回 -1
 */
static inline int conn_set_read_handler(struct ae_event_loop_t *el, connection *conn, ConnectionCallbackFunc func) {
    return conn->type->set_read_handler(el, conn, func);
}

/**
 * @brief 注册写事件处理器（不启用写屏障）
 * @param el   事件循环指针
 * @param conn 目标连接指针
 * @param func 写事件回调（NULL 表示移除）
 * @return int 成功返回 0，失败返回 -1
 */
static inline int conn_set_write_handler(struct ae_event_loop_t *el, connection *conn, ConnectionCallbackFunc func) {
    return conn->type->set_write_handler(el, conn, func, 0);
}

/**
 * @brief 从连接读取数据（行为同 read(2)）
 *
 * 可能发生短读；返回 0 表示连接已关闭，-1 表示出错。
 * 调用方不应依赖 errno，应通过 conn_get_state() 检查连接状态。
 * @param conn    目标连接指针
 * @param buf     接收缓冲区
 * @param buf_len 缓冲区大小
 * @return int 实际读取字节数，0 表示连接关闭，-1 表示出错
 */
static inline int conn_read(connection *conn, void *buf, size_t buf_len) {
    return conn->type->read(conn, buf, buf_len);
}

/**
 * @brief 获取连接的地址信息（本地或远端）
 * @param conn   目标连接指针
 * @param ip     输出：IP 地址字符串
 * @param ip_len IP 缓冲区大小
 * @param port   输出：端口号
 * @param remote 1=获取远端地址，0=获取本地地址
 * @return int 成功返回 0，失败返回 -1
 */
static inline int conn_addr(connection *conn, char *ip, size_t ip_len, int *port, int remote) {
    if (conn && conn->type->addr) {
        return conn->type->addr(conn, ip, ip_len, port, remote);
    }

    return -1;
}

/**
 * @brief 将 IP 和端口格式化为 "ip:port" 或 "[ipv6]:port" 字符串
 * @param buf     输出缓冲区
 * @param buf_len 缓冲区大小
 * @param ip      IP 地址字符串
 * @param port    端口号
 * @return int snprintf 写入字节数
 */
static inline int format_addr(char *buf, size_t buf_len, char *ip, int port) {
    return snprintf(buf, buf_len, strchr(ip,':') ?
           "[%s]:%d" : "%s:%d", ip, port);
}

/** @brief 连接地址字符串缓冲区大小 */
#define CONN_ADDR_STR_LEN 128

/**
 * @brief 获取连接的格式化地址字符串
 * @param conn    目标连接指针
 * @param buf     输出缓冲区
 * @param buf_len 缓冲区大小
 * @param remote  1=远端地址，0=本地地址
 * @return int 成功返回写入字节数，失败返回 -1
 */
static inline int conn_format_addr(connection *conn, char *buf, size_t buf_len, int remote)
{
    char ip[CONN_ADDR_STR_LEN];
    int port;

    if (conn_addr(conn, ip, sizeof(ip), &port, remote) < 0) {
        return -1;
    }

    return format_addr(buf, buf_len, ip, port);
}

#endif  /* __REDIS_CONNECTION_H */
