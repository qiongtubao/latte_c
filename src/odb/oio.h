#ifndef __LATTE_OIO_H__

#define __LATTE_OIO_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include "sds/sds.h"
#include "connection/connection.h"

/**
 * @brief 通用 I/O 抽象结构体（Object I/O）
 *
 * 提供统一的读/写/定位/刷新接口，支持多种后端：
 * - buffer：基于 sds 的内存缓冲（序列化/反序列化场景）
 * - file：基于 FILE* 的文件 I/O
 * - conn：基于 connection 的网络 socket
 * - fd：基于文件描述符的管道写入
 * - connset：多连接广播写入
 *
 * 所有读写函数均不容忍短读/短写，返回值简化为：
 * - 0 表示错误
 * - 非 0 表示完全成功
 */
struct _oio {
    /**
     * @brief 读取 len 字节到 buf，失败返回 0，成功返回 len
     */
    size_t (*read)(struct _oio *, void *buf, size_t len);

    /**
     * @brief 写入 len 字节到后端，失败返回 0，成功返回 len
     */
    size_t (*write)(struct _oio *, const void *buf, size_t len);

    /**
     * @brief 返回当前读写位置（字节偏移）
     */
    off_t (*tell)(struct _oio *);

    /**
     * @brief 刷新缓冲（如 fflush），成功返回非 0
     */
    int (*flush)(struct _oio *);

    /**
     * @brief 校验和更新回调（可选），用于计算所有读写数据的 checksum
     * 若不为 NULL，每次读写后以当前 cksum、buf、len 为参数调用
     */
    void (*update_cksum)(struct _oio *, const void *buf, size_t len);

    uint64_t cksum;           /**< 当前校验和 */
    uint64_t flags;           /**< 标志位（RIO_FLAG_* 系列） */
    size_t processed_bytes;   /**< 已读或已写的总字节数 */
    size_t max_processing_chunk; /**< 单次读写的最大字节数上限 */

    /** @brief 后端特定数据，使用 union 节省内存 */
    union {
        /** 内存 buffer 后端：基于 sds 的动态缓冲区 */
        struct {
            sds ptr;    /**< sds 缓冲区指针 */
            off_t pos;  /**< 当前读写位置 */
        } buffer;

        /** 文件后端：基于 FILE* */
        struct {
            FILE *fp;           /**< 文件指针 */
            off_t buffered;     /**< 自上次 fsync 以来已写入的字节数 */
            off_t autosync;     /**< 达到该字节数时自动 fsync */
            unsigned reclaim_cache:1; /**< fsync 后是否回收缓存 */
        } file;

        /** 连接后端：基于 connection（用于 socket 读取） */
        struct {
            connection *conn;     /**< 网络连接对象 */
            off_t pos;            /**< 在 buf 中的当前读取位置 */
            sds buf;              /**< 已缓冲的数据 */
            size_t read_limit;    /**< 最大允许缓冲/读取的字节数 */
            size_t read_so_far;   /**< 已从 rio 读取的字节数（不计缓冲） */
        } conn;

        /** 文件描述符后端：用于管道写入 */
        struct {
            int fd;    /**< 文件描述符 */
            off_t pos; /**< 当前写入位置 */
            sds buf;   /**< 写缓冲 */
        } fd;

        /** 多连接广播后端：向 N 个 socket 同时写入 */
        struct {
            struct {
                connection *conn; /**< 目标连接 */
                int failed;       /**< 该连接是否写入失败 */
            } *dst;               /**< 目标连接数组 */
            size_t n_dst;         /**< 连接数量 */
            off_t pos;            /**< 已发送字节数 */
            sds buf;              /**< 发送缓冲 */
        } connset;
    } io;
};

/** @brief oio 结构体类型别名 */
typedef struct _oio oio;
#endif