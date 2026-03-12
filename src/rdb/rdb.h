#ifndef __LATTE_RDB_H
#define __LATTE_RDB_H

#include <stdio.h>
#include <stdint.h>
#include "sds/sds.h"
#include "object/object.h"
#include "object/module.h"

/** @brief RDB 文件格式版本号 */
#define RDB_VERSION 1

/* ==================== RDB 操作码（Opcode）==================== */
/** @brief 文件结束标志 */
#define RDB_OPCODE_EOF          255
/** @brief 切换数据库操作码 */
#define RDB_OPCODE_SELECTDB     254
/** @brief 过期时间（秒）操作码 */
#define RDB_OPCODE_EXPIRETIME   253
/** @brief 过期时间（毫秒）操作码 */
#define RDB_OPCODE_EXPIRETIME_MS 252
/** @brief 数据库 resize 操作码 */
#define RDB_OPCODE_RESIZEDB     251
/** @brief 辅助字段操作码 */
#define RDB_OPCODE_AUX          250
/** @brief 模块辅助数据操作码 */
#define RDB_OPCODE_MODULE_AUX   247

/* ==================== IO 类型 ==================== */
/** @brief 文件 IO 模式 */
#define RDB_IO_FILE   1
/** @brief 内存缓冲区 IO 模式 */
#define RDB_IO_BUFFER 2

/** @brief 前向声明（模块 IO 结构体） */
struct RedisModuleIO;

/* ==================== IO 函数指针类型 ==================== */
/** @brief 读取函数类型：从 IO 对象读取 len 字节到 buf，返回实际读取字节数 */
typedef size_t (*rdb_read_func)(struct RedisModuleIO *io, void *buf, size_t len);
/** @brief 写入函数类型：将 buf 中 len 字节写入 IO 对象，返回实际写入字节数 */
typedef size_t (*rdb_write_func)(struct RedisModuleIO *io, const void *buf, size_t len);
/** @brief 位置查询函数类型：返回当前 IO 偏移量 */
typedef off_t  (*rdb_tell_func)(struct RedisModuleIO *io);
/** @brief 刷新函数类型：将缓冲数据刷写到底层，成功返回 0 */
typedef int    (*rdb_flush_func)(struct RedisModuleIO *io);

/**
 * @brief RDB IO 上下文结构体（兼容 Redis 模块 IO 接口）
 *
 * 支持两种后端：
 *   - RDB_IO_FILE：基于 FILE* 的文件 IO
 *   - RDB_IO_BUFFER：基于 SDS 字符串的内存缓冲区 IO
 *
 * 虚函数表（read/write/tell/flush）在初始化时按类型赋值，
 * 调用方只需通过统一接口操作，无需关心底层实现。
 */
typedef struct RedisModuleIO {
    int type;                   /**< IO 类型：RDB_IO_FILE 或 RDB_IO_BUFFER */
    uint64_t bytes_processed;   /**< 累计已处理字节数（读或写） */
    uint64_t checksum;          /**< 当前校验和（用于数据完整性验证） */
    int error;                  /**< 错误标志，非 0 表示发生错误 */

    /** @brief IO 后端联合体，根据 type 使用对应字段 */
    union {
        struct {
            FILE *fp;       /**< 文件指针（RDB_IO_FILE 模式） */
            off_t written;  /**< 已写入字节数（用于校验） */
        } file;
        struct {
            sds ptr;        /**< SDS 缓冲区指针（RDB_IO_BUFFER 模式） */
            off_t pos;      /**< 当前读取/写入偏移量 */
        } buffer;
    } io;

    /* ==================== 虚函数表 ==================== */
    rdb_read_func  read;  /**< 读取函数指针，由初始化函数设置 */
    rdb_write_func write; /**< 写入函数指针，由初始化函数设置 */
    rdb_tell_func  tell;  /**< 偏移查询函数指针 */
    rdb_flush_func flush; /**< 刷新函数指针 */

    void *ctx; /**< 模块自定义上下文指针（可选，用于扩展） */
} RedisModuleIO;

/* ==================== 初始化函数 ==================== */

/**
 * @brief 以文件模式初始化 RedisModuleIO
 * @param io 目标 IO 结构体指针
 * @param fp 已打开的文件指针
 */
void rdb_init_io_file(RedisModuleIO *io, FILE *fp);

/**
 * @brief 以内存缓冲区模式初始化 RedisModuleIO
 * @param io     目标 IO 结构体指针
 * @param buffer 初始 SDS 缓冲区（传 NULL 时从空缓冲区开始）
 */
void rdb_init_io_buffer(RedisModuleIO *io, sds buffer);

/**
 * @brief 获取缓冲区模式下的 SDS 缓冲区指针
 * @param io 目标 IO 结构体指针（必须是 RDB_IO_BUFFER 模式）
 * @return sds 当前 SDS 缓冲区内容
 */
sds rdb_get_buffer(RedisModuleIO *io);

/* ==================== 基本 RDB 写入操作 ==================== */

/**
 * @brief 写入一个类型字节（单字节操作码或对象类型）
 * @param io   目标 IO 对象
 * @param type 类型字节
 * @return ssize_t 成功返回写入字节数，失败返回 -1
 */
ssize_t rdb_save_type(RedisModuleIO *io, unsigned char type);

/**
 * @brief 写入一个变长长度编码整数
 * @param io  目标 IO 对象
 * @param len 要编码的长度值
 * @return ssize_t 写入字节数，失败返回 -1
 */
ssize_t rdb_save_len(RedisModuleIO *io, uint64_t len);

/**
 * @brief 写入一个字符串（带长度前缀）
 * @param io  目标 IO 对象
 * @param str 字符串数据指针
 * @param len 字符串长度
 * @return ssize_t 写入字节数，失败返回 -1
 */
ssize_t rdb_save_string(RedisModuleIO *io, const char *str, size_t len);

/**
 * @brief 写入一段二进制数据（带长度前缀）
 * @param io  目标 IO 对象
 * @param s   二进制数据指针
 * @param len 数据长度
 * @return ssize_t 写入字节数，失败返回 -1
 */
ssize_t rdb_save_binary_string(RedisModuleIO *io, const unsigned char *s, size_t len);

/**
 * @brief 将 long long 整数以字符串形式写入
 * @param io    目标 IO 对象
 * @param value 整数值
 * @return ssize_t 写入字节数，失败返回 -1
 */
ssize_t rdb_save_long_long_as_string(RedisModuleIO *io, long long value);

/**
 * @brief 序列化并写入一个 latte_object_t 对象
 * @param io 目标 IO 对象
 * @param o  要保存的对象指针
 * @return ssize_t 写入字节数，失败返回 -1
 */
ssize_t rdb_save_object(RedisModuleIO *io, latte_object_t *o);

/* ==================== 基本 RDB 读取操作 ==================== */

/**
 * @brief 读取一个类型字节
 * @param io 目标 IO 对象
 * @return int 类型字节值，失败返回 -1
 */
int rdb_load_type(RedisModuleIO *io);

/**
 * @brief 读取一个变长长度编码整数
 * @param io        目标 IO 对象
 * @param isencoded 输出参数：非 0 表示该长度值是一个编码整数
 * @return uint64_t 解码后的长度值
 */
uint64_t rdb_load_len(RedisModuleIO *io, int *isencoded);

/**
 * @brief 读取一个字符串（带长度前缀）
 * @param io 目标 IO 对象
 * @return sds 读取到的 SDS 字符串，失败返回 NULL
 */
sds rdb_load_string(RedisModuleIO *io);

/**
 * @brief 从 RDB 中加载指定类型的对象
 * @param io   目标 IO 对象
 * @param type 对象类型（由 rdb_load_type 读取）
 * @return latte_object_t* 加载的对象，失败返回 NULL
 */
latte_object_t *rdb_load_object(RedisModuleIO *io, int type);

/* ==================== 高级 API ==================== */

/**
 * @brief 将对象序列化保存到 RDB 文件
 * @param filename 目标 RDB 文件路径
 * @param obj      要保存的对象指针
 * @return int 成功返回 0，失败返回 -1
 */
int rdb_save(const char *filename, latte_object_t *obj);

#endif
