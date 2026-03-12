/**
 * @file anet.h
 * @brief 网络工具函数接口
 *        封装 TCP/Unix Socket 的创建、连接、监听、接受、属性设置等常用操作，
 *        提供统一的错误字符串输出接口（err 参数）。
 */
#ifndef __LATTE_ANET_H
#define __LATTE_ANET_H


#include <sys/types.h>

/** @brief 操作成功返回值 */
#define ANET_OK 0
/** @brief 操作失败返回值 */
#define ANET_ERR -1
/** @brief 错误信息字符串最大长度 */
#define ANET_ERR_LEN 256

/* Flags used with certain functions. */
/** @brief 无标志 */
#define ANET_NONE 0
/** @brief 仅解析 IP 地址，不做 DNS 查询 */
#define ANET_IP_ONLY (1<<0)

#if defined(__sun) || defined(_AIX)
#define AF_LOCAL AF_UNIX
#endif

#ifdef _AIX
#undef ip_len
#endif

/* FD to address string conversion types */
/** @brief 获取对端（peer）地址 */
#define FD_TO_PEER_NAME 0
/** @brief 获取本端（sock）地址 */
#define FD_TO_SOCK_NAME 1

/**
 * @brief 以非阻塞方式发起 TCP 连接
 * @param err  错误信息输出缓冲区（ANET_ERR_LEN 字节）
 * @param addr 目标主机地址（IP 或域名）
 * @param port 目标端口号
 * @return 成功返回已连接的 fd；失败返回 ANET_ERR 并填写 err
 */
int anetTcpNonBlockConnect(char *err, const char *addr, int port);

/**
 * @brief 以非阻塞方式发起 TCP 连接，并尽力绑定指定源地址
 * @param err         错误信息输出缓冲区
 * @param addr        目标主机地址
 * @param port        目标端口号
 * @param source_addr 本地绑定地址（NULL 表示不指定）
 * @return 成功返回 fd；失败返回 ANET_ERR 并填写 err
 */
int anetTcpNonBlockBestEffortBindConnect(char *err, const char *addr, int port, const char *source_addr);

/**
 * @brief 将主机名解析为 IP 字符串
 * @param err       错误信息输出缓冲区
 * @param host      待解析的主机名或 IP 字符串
 * @param ipbuf     输出 IP 字符串的缓冲区
 * @param ipbuf_len ipbuf 的字节长度
 * @param flags     解析标志（如 ANET_IP_ONLY）
 * @return 成功返回 ANET_OK；失败返回 ANET_ERR 并填写 err
 */
int anetResolve(char *err, char *host, char *ipbuf, size_t ipbuf_len, int flags);

/**
 * @brief 创建 IPv4 TCP 监听套接字
 * @param err      错误信息输出缓冲区
 * @param port     监听端口号
 * @param bindaddr 绑定地址（NULL 表示 INADDR_ANY）
 * @param backlog  listen() 队列长度
 * @return 成功返回监听 fd；失败返回 ANET_ERR 并填写 err
 */
int anetTcpServer(char *err, int port, char *bindaddr, int backlog);

/**
 * @brief 创建 IPv6 TCP 监听套接字
 * @param err      错误信息输出缓冲区
 * @param port     监听端口号
 * @param bindaddr 绑定地址（NULL 表示 in6addr_any）
 * @param backlog  listen() 队列长度
 * @return 成功返回监听 fd；失败返回 ANET_ERR 并填写 err
 */
int anetTcp6Server(char *err, int port, char *bindaddr, int backlog);

/**
 * @brief 创建 Unix 域套接字监听端
 * @param err     错误信息输出缓冲区
 * @param path    套接字文件路径
 * @param perm    套接字文件权限（mode_t）
 * @param backlog listen() 队列长度
 * @return 成功返回监听 fd；失败返回 ANET_ERR 并填写 err
 */
int anetUnixServer(char *err, char *path, mode_t perm, int backlog);

/**
 * @brief 在 TCP 监听套接字上接受新连接
 * @param err        错误信息输出缓冲区
 * @param serversock 监听套接字 fd
 * @param ip         输出客户端 IP 字符串的缓冲区
 * @param ip_len     ip 缓冲区字节长度
 * @param port       输出客户端端口号
 * @return 成功返回新连接 fd；失败返回 ANET_ERR 并填写 err
 */
int anetTcpAccept(char *err, int serversock, char *ip, size_t ip_len, int *port);

/**
 * @brief 在 Unix 域监听套接字上接受新连接
 * @param err        错误信息输出缓冲区
 * @param serversock 监听套接字 fd
 * @return 成功返回新连接 fd；失败返回 ANET_ERR 并填写 err
 */
int anetUnixAccept(char *err, int serversock);

/**
 * @brief 将 fd 设置为非阻塞模式（O_NONBLOCK）
 * @param err 错误信息输出缓冲区
 * @param fd  目标文件描述符
 * @return 成功返回 ANET_OK；失败返回 ANET_ERR 并填写 err
 */
int anetNonBlock(char *err, int fd);

/**
 * @brief 将 fd 设置为阻塞模式（清除 O_NONBLOCK）
 * @param err 错误信息输出缓冲区
 * @param fd  目标文件描述符
 * @return 成功返回 ANET_OK；失败返回 ANET_ERR 并填写 err
 */
int anetBlock(char *err, int fd);

/**
 * @brief 为 fd 设置 close-on-exec 标志（FD_CLOEXEC）
 * @param fd 目标文件描述符
 * @return 成功返回 ANET_OK；失败返回 ANET_ERR
 */
int anetCloexec(int fd);

/**
 * @brief 启用 TCP_NODELAY（禁用 Nagle 算法）
 * @param err 错误信息输出缓冲区
 * @param fd  目标套接字 fd
 * @return 成功返回 ANET_OK；失败返回 ANET_ERR 并填写 err
 */
int anetEnableTcpNoDelay(char *err, int fd);

/**
 * @brief 禁用 TCP_NODELAY（启用 Nagle 算法）
 * @param err 错误信息输出缓冲区
 * @param fd  目标套接字 fd
 * @return 成功返回 ANET_OK；失败返回 ANET_ERR 并填写 err
 */
int anetDisableTcpNoDelay(char *err, int fd);

/**
 * @brief 设置套接字发送超时时间
 * @param err 错误信息输出缓冲区
 * @param fd  目标套接字 fd
 * @param ms  超时毫秒数
 * @return 成功返回 ANET_OK；失败返回 ANET_ERR 并填写 err
 */
int anetSendTimeout(char *err, int fd, long long ms);

/**
 * @brief 设置套接字接收超时时间
 * @param err 错误信息输出缓冲区
 * @param fd  目标套接字 fd
 * @param ms  超时毫秒数
 * @return 成功返回 ANET_OK；失败返回 ANET_ERR 并填写 err
 */
int anetRecvTimeout(char *err, int fd, long long ms);

/**
 * @brief 将 fd 的地址信息转换为 IP + 端口字符串
 * @param fd              目标套接字 fd
 * @param ip              输出 IP 字符串的缓冲区
 * @param ip_len          ip 缓冲区字节长度
 * @param port            输出端口号（可为 NULL）
 * @param fd_to_str_type  转换类型（FD_TO_PEER_NAME / FD_TO_SOCK_NAME）
 * @return 成功返回 ANET_OK；失败返回 ANET_ERR
 */
int anetFdToString(int fd, char *ip, size_t ip_len, int *port, int fd_to_str_type);

/**
 * @brief 为 TCP 连接启用 TCP keepalive 并设置探活间隔
 * @param err      错误信息输出缓冲区
 * @param fd       目标套接字 fd
 * @param interval 探活间隔秒数
 * @return 成功返回 ANET_OK；失败返回 ANET_ERR 并填写 err
 */
int anetKeepAlive(char *err, int fd, int interval);

/**
 * @brief 将 IP + 端口格式化为 "ip:port" 字符串
 * @param fmt     输出缓冲区
 * @param fmt_len 缓冲区字节长度
 * @param ip      IP 地址字符串
 * @param port    端口号
 * @return 格式化后字符串长度；失败返回 ANET_ERR
 */
int anetFormatAddr(char *fmt, size_t fmt_len, char *ip, int port);

/**
 * @brief 将 fd 对应的地址格式化为字符串
 * @param fd              目标套接字 fd
 * @param buf             输出缓冲区
 * @param buf_len         缓冲区字节长度
 * @param fd_to_str_type  转换类型（FD_TO_PEER_NAME / FD_TO_SOCK_NAME）
 * @return 格式化后字符串长度；失败返回 ANET_ERR
 */
int anetFormatFdAddr(int fd, char *buf, size_t buf_len, int fd_to_str_type);



#endif
