
#ifndef __LATTE_CLIENT_H
#define __LATTE_CLIENT_H

#include "server.h"
#include "time/localtime.h"

/* ==================== 客户端标志位 ==================== */
#define CLIENT_CLOSE_AFTER_REPLY (1<<0) /**< 发送完整回复后关闭连接 */
#define CLIENT_CLOSE_ASAP        (1<<1) /**< 尽快关闭此客户端（异步关闭标记） */
#define CLIENT_PROTECTED         (1<<2) /**< 客户端受保护，暂时不可释放 */
#define CLIENT_PENDING_WRITE     (1<<3) /**< 客户端有待发送数据，但写事件处理器尚未安装 */

/* ==================== 协议类型 ==================== */
#define PROTO_REQ_INLINE     1  /**< 内联协议请求 */
#define PROTO_REQ_MULTIBULK  2  /**< 多批量协议请求（RESP 协议） */

/* ==================== 协议缓冲区大小常量 ==================== */
#define PROTO_IOBUF_LEN         (1024*16)  /**< 通用 IO 缓冲区大小（16KB） */
#define PROTO_REPLY_CHUNK_BYTES (16*1024)  /**< 输出缓冲区块大小（16KB） */
#define PROTO_INLINE_MAX_SIZE   (1024*64)  /**< 内联读取最大尺寸（64KB） */
#define PROTO_MBULK_BIG_ARG     (1024*32)  /**< 多批量大参数阈值（32KB） */

/** @brief 单次事件最大写入字节数（64KB） */
#define NET_MAX_WRITES_PER_EVENT (1024*64)

/** @brief 异步 IO 缓冲区最大尺寸（16KB） */
#define CLIENT_ASYNC_IO_MAX_SIZE  (1024*16)

/**
 * @brief 客户端输出缓冲区块结构体
 *
 * reply 链表中的每个节点均为此结构，buf[] 为柔性数组存储实际数据。
 */
typedef struct client_reply_block_t {
    size_t size;    /**< buf[] 的总容量 */
    size_t used;    /**< buf[] 中已使用的字节数 */
    char buf[];     /**< 实际数据缓冲区（柔性数组） */
} client_reply_block_t;

/* ==================== 客户端回调函数类型 ==================== */
/**
 * @brief 请求处理函数类型：解析并执行客户端命令
 * @param client 客户端指针
 * @param nread  本次读取的字节数
 * @return int   成功返回非 0，失败返回 0
 */
typedef int (*handle_func)(struct latte_client_t* client, int nread);

/**
 * @brief 请求开始回调：连接读取数据前触发（用于统计等）
 * @param client 客户端指针
 */
typedef void (*start_func)(struct latte_client_t* client);

/**
 * @brief 请求结束回调：数据写入完成后触发（用于统计等）
 * @param client 客户端指针
 */
typedef void (*end_func)(struct latte_client_t* client);

/**
 * @brief latte 客户端核心结构体
 *
 * 代表一个 TCP 连接客户端，包含查询缓冲区、回复缓冲区、
 * 连接状态、统计时间戳等全部运行时状态。
 */
typedef struct latte_client_t {
    sds name;                   /**< 客户端名称（可选，用于调试） */
    sds peer_id;                /**< 对端地址字符串（ip:port 格式，懒加载） */
    uint64_t id;                /**< 客户端唯一 ID（原子递增分配） */
    connection *conn;           /**< 底层连接对象指针 */

    /* ===== 查询缓冲区 ===== */
    sds_t querybuf;             /**< 累积客户端查询数据的 SDS 缓冲区 */
    size_t qb_pos;              /**< querybuf 中已处理的偏移位置 */
    size_t querybuf_peak;       /**< 查询缓冲区历史峰值大小（用于监控） */

    /* ===== 请求处理回调 ===== */
    handle_func exec;           /**< 命令解析与执行回调 */
    start_func start;           /**< 请求开始回调（可为 NULL） */
    end_func end;               /**< 请求结束回调（可为 NULL） */

    int flags;                  /**< 客户端标志位（CLIENT_* 宏的组合） */
    struct latte_server_t* server;          /**< 所属服务器指针 */
    struct list_node_t* client_list_node;   /**< 在 server->clients 链表中的节点（O(1) 删除） */

    /* ===== 输出缓冲区 ===== */
    int bufpos;                             /**< buf[] 中已写入的字节数 */
    char buf[PROTO_REPLY_CHUNK_BYTES];      /**< 固定输出缓冲区（16KB） */
    list_t* reply;                          /**< 溢出回复链表（存储 client_reply_block_t*） */
    size_t reply_bytes;                     /**< reply 链表中已分配的总字节数 */
    size_t sentlen;                         /**< 当前缓冲区/块已发送的字节数 */

    /* ===== 异步 IO ===== */
    async_io_request_t* async_io_request_cache; /**< 异步 IO 写请求缓存（复用，避免频繁分配） */
    struct list_node_t* async_io_client_node;   /**< 在 clients_async_pending_write 中的节点 */

    /* ===== 请求阶段时间戳（用于性能统计） ===== */
    ustime_point_t start_time;      /**< 请求开始时间（读取数据前） */
    ustime_point_t read_time;       /**< 数据读取完成时间 */
    ustime_point_t exec_time;       /**< 命令执行开始时间 */
    ustime_point_t exec_end_time;   /**< 命令执行结束时间 */
    ustime_point_t write_time;      /**< 回复写入开始时间 */
    ustime_point_t end_time;        /**< 回复发送完成时间 */
} latte_client_t;

/* ==================== 客户端生命周期接口 ==================== */

/**
 * @brief 初始化客户端的保护字段（查询缓冲区、回复链表、异步 IO 缓存等）
 * 在 createClient 返回后、连接建立前调用，确保字段安全初始化。
 * @param client 目标客户端指针
 */
void protected_init_latte_client(latte_client_t* client);

/**
 * @brief 销毁客户端内部资源（释放 querybuf 等）
 * @param client 目标客户端指针
 */
void destory_latte_client(latte_client_t* client);

/**
 * @brief 将客户端信息序列化为 SDS 字符串（用于 CLIENT INFO 命令等）
 * @param client 目标客户端指针
 * @return sds_t 描述字符串（调用方负责释放）
 */
sds_t latte_client_info_to_sds(latte_client_t* client);

/**
 * @brief 向客户端追加回复数据（自动选择同步/异步 IO 路径）
 * @param client 目标客户端指针
 * @param data   回复数据指针
 * @param len    回复数据长度
 */
void add_reply_proto(latte_client_t* client, const char* data, size_t len);

/**
 * @brief 将客户端链接到服务器的客户端列表和索引中
 * @param server 目标服务器指针
 * @param c      目标客户端指针
 */
void link_latte_client(latte_server_t* server, latte_client_t* c);

/**
 * @brief 从服务器客户端列表和索引中移除客户端，并关闭连接
 * @param c 目标客户端指针
 */
void unlink_latte_client(latte_client_t* c);

/**
 * @brief 完成客户端初始化，调用 connAccept 触发握手或直接连接
 * @param el    事件循环指针
 * @param c     目标客户端指针
 * @param conn  底层连接对象
 * @param flags 初始客户端标志
 */
void init_latte_client(struct ae_event_loop_t* el, latte_client_t* c, struct connection* conn, int flags);

/**
 * @brief 异步释放客户端（当前实现直接调用 free_latte_client，TODO 实现真正异步）
 * @param c 目标客户端指针
 */
void free_latte_client_async(latte_client_t *c);

/**
 * @brief 同步释放客户端：unlink → 销毁内部资源 → 调用 server->freeClient
 * @param c 目标客户端指针
 */
void free_latte_client(latte_client_t *c);

/**
 * @brief 将客户端输出缓冲区数据写入连接 socket
 * @param c                 目标客户端指针
 * @param handler_installed 非 0 表示写事件处理器已安装，写完后需卸载
 * @return int 成功返回 0，连接已断开或出错返回 -1
 */
int write_to_client(latte_client_t *c, int handler_installed);

/**
 * @brief 检查客户端是否还有待发送的回复数据
 * @param c 目标客户端指针
 * @return int 有待发送数据返回非 0，否则返回 0
 */
int client_has_pending_replies(latte_client_t *c);

/**
 * @brief 尝试通过异步 IO 写入客户端缓存的回复数据
 * @param c 目标客户端指针
 * @return int 成功提交异步 IO 返回 1，降级到同步 IO 返回 0
 */
int async_io_try_write(latte_client_t* c);

/**
 * @brief 获取客户端名称
 * @param c 目标客户端指针
 * @return sds 客户端名称字符串（可能为 NULL）
 */
sds client_get_name(latte_client_t* c);

/**
 * @brief 获取客户端对端地址字符串（懒加载，首次调用时生成）
 * @param c 目标客户端指针
 * @return sds 对端地址字符串（"ip:port" 格式）
 */
sds client_get_peer_id(latte_client_t* c);
#endif