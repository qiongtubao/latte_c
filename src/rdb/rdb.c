#include "rdb.h"
#include <arpa/inet.h>
#include "zmalloc/zmalloc.h"
#include "crc/crc64.h" /* Disabled for now as file found issues */
#include <string.h>
#include "log/log.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* ==================== IO 抽象层实现 ==================== */

/** @brief 长度解码错误时的返回值（UINT64_MAX） */
#define RDB_LENERR UINT64_MAX

/**
 * @brief 文件 IO 写入实现：通过 fwrite 写入数据
 * @param io  IO 上下文
 * @param buf 数据缓冲区
 * @param len 写入字节数
 * @return size_t 实际写入字节数
 */
static size_t rdb_file_write(RedisModuleIO *io, const void *buf, size_t len) {
    return fwrite(buf, 1, len, io->io.file.fp);
}

/**
 * @brief 文件 IO 读取实现：通过 fread 读取数据
 * @param io  IO 上下文
 * @param buf 目标缓冲区
 * @param len 期望读取字节数
 * @return size_t 实际读取字节数
 */
static size_t rdb_file_read(RedisModuleIO *io, void *buf, size_t len) {
    return fread(buf, 1, len, io->io.file.fp);
}

/**
 * @brief 文件 IO 偏移查询：返回当前文件偏移量
 * @param io IO 上下文
 * @return off_t 当前文件位置
 */
static off_t rdb_file_tell(RedisModuleIO *io) {
    return ftello(io->io.file.fp);
}

/**
 * @brief 文件 IO 刷新：将缓冲数据写入磁盘
 * @param io IO 上下文
 * @return int 成功返回 0，失败返回 EOF
 */
static int rdb_file_flush(RedisModuleIO *io) {
    return fflush(io->io.file.fp);
}

void *memcpy(void *dest, const void *src, size_t n);

/**
 * @brief 缓冲区 IO 写入实现：将数据追加到 SDS 缓冲区
 * @param io  IO 上下文
 * @param buf 数据指针
 * @param len 写入长度
 * @return size_t 实际写入字节数
 */
static size_t rdb_buffer_write(RedisModuleIO *io, const void *buf, size_t len) {
    /* 通过 sds_cat_len 追加到 SDS 缓冲区，并更新位置偏移 */
    io->io.buffer.ptr = sds_cat_len(io->io.buffer.ptr, buf, len);
    io->io.buffer.pos += len;
    return len;
}

/**
 * @brief 缓冲区 IO 读取实现：从 SDS 缓冲区当前位置读取数据
 * 读取量受剩余可用字节数限制，不会越界。
 * @param io  IO 上下文
 * @param buf 目标缓冲区
 * @param len 期望读取字节数
 * @return size_t 实际读取字节数（不超过剩余字节数）
 */
static size_t rdb_buffer_read(RedisModuleIO *io, void *buf, size_t len) {
    /* 计算剩余可读字节数，防止越界 */
    size_t avail = sds_len(io->io.buffer.ptr) - io->io.buffer.pos;
    if (len > avail) len = avail;
    if (len > 0) {
        memcpy(buf, io->io.buffer.ptr + io->io.buffer.pos, len);
        io->io.buffer.pos += len;
    }
    return len;
}

/**
 * @brief 缓冲区 IO 偏移查询：返回当前读写位置
 * @param io IO 上下文
 * @return off_t 当前缓冲区偏移
 */
static off_t rdb_buffer_tell(RedisModuleIO *io) {
    return io->io.buffer.pos;
}

/**
 * @brief 缓冲区 IO 刷新：内存操作无需刷新，直接返回 0
 * @param io IO 上下文（未使用）
 * @return int 始终返回 0
 */
static int rdb_buffer_flush(RedisModuleIO *io) {
    return 0;
}

/**
 * @brief 以文件模式初始化 IO 上下文
 * 将虚函数表指向文件 IO 实现，清零统计字段。
 * @param io IO 上下文指针
 * @param fp 已打开的文件指针
 */
void rdb_init_io_file(RedisModuleIO *io, FILE *fp) {
    io->type = RDB_IO_FILE;
    io->io.file.fp = fp;
    io->io.file.written = 0;
    io->read  = rdb_file_read;
    io->write = rdb_file_write;
    io->tell  = rdb_file_tell;
    io->flush = rdb_file_flush;
    io->bytes_processed = 0;
    io->checksum = 0;
    io->error = 0;
}

/**
 * @brief 以内存缓冲区模式初始化 IO 上下文
 * buffer 为 NULL 时创建空 SDS 缓冲区，否则使用传入的 SDS。
 * @param io     IO 上下文指针
 * @param buffer 初始 SDS 缓冲区（可为 NULL）
 */
void rdb_init_io_buffer(RedisModuleIO *io, sds buffer) {
    io->type = RDB_IO_BUFFER;
    io->io.buffer.ptr = buffer ? buffer : sds_empty(); /* NULL 时创建空缓冲区 */
    io->io.buffer.pos = 0;
    io->read  = rdb_buffer_read;
    io->write = rdb_buffer_write;
    io->tell  = rdb_buffer_tell;
    io->flush = rdb_buffer_flush;
    io->bytes_processed = 0;
    io->checksum = 0;
    io->error = 0;
}

/**
 * @brief 获取缓冲区模式下的 SDS 指针
 * @param io IO 上下文（必须是 RDB_IO_BUFFER 模式）
 * @return sds 缓冲区 SDS 指针，非缓冲区模式返回 NULL
 */
sds rdb_get_buffer(RedisModuleIO *io) {
    if (io->type == RDB_IO_BUFFER) {
        return io->io.buffer.ptr;
    }
    return NULL;
}

/* ==================== 底层读写辅助函数 ==================== */

/**
 * @brief 原始写入：检查错误标志，写入数据并更新 bytes_processed
 * @param io  IO 上下文
 * @param p   数据指针
 * @param len 写入字节数
 * @return ssize_t 成功返回 len，失败（写入不完整或已有错误）返回 -1
 */
static ssize_t rdb_write_raw(RedisModuleIO *io, const void *p, size_t len) {
    if (io->error) return -1; /* 已有错误，拒绝继续写入 */
    size_t nwritten = io->write(io, p, len);
    if (nwritten != len) {
        io->error = 1; /* 标记错误 */
        return -1;
    }
    io->bytes_processed += len;
    /* CRC 校验暂未启用：io->checksum = crc64(io->checksum, p, len); */
    return len;
}

/**
 * @brief 原始读取：检查错误标志，读取数据并更新 bytes_processed
 * @param io  IO 上下文
 * @param p   目标缓冲区
 * @param len 期望读取字节数
 * @return ssize_t 成功返回 len，失败返回 -1
 */
static ssize_t rdb_read_raw(RedisModuleIO *io, void *p, size_t len) {
    if (io->error) return -1;
    size_t nread = io->read(io, p, len);
    if (nread != len) {
        io->error = 1;
        return -1;
    }
    io->bytes_processed += len;
    return len;
}

/**
 * @brief 写入一个类型字节（单字节操作码）
 * @param io   IO 上下文
 * @param type 类型字节值
 * @return ssize_t 成功返回 1，失败返回 -1
 */
ssize_t rdb_save_type(RedisModuleIO *io, unsigned char type) {
    return rdb_write_raw(io, &type, 1);
}

/**
 * @brief 读取一个类型字节
 * @param io IO 上下文
 * @return int 类型字节值（0~255），失败返回 -1
 */
int rdb_load_type(RedisModuleIO *io) {
    unsigned char type;
    if (rdb_read_raw(io, &type, 1) == -1) return -1;
    return type;
}

/**
 * @brief 写入变长长度编码整数（Length Encoding）
 *
 * 编码规则（与 Redis RDB 兼容）：
 *   - len < 64        → 1 字节：高 2 位 = 00，低 6 位 = len
 *   - len < 16384     → 2 字节：高 2 位 = 01，剩余 14 位 = len
 *   - len <= UINT32MAX → 5 字节：标志 0x80 + 网络序 uint32
 *   - len > UINT32MAX → 9 字节：标志 0xC1 + 网络序 uint64
 *
 * @param io  IO 上下文
 * @param len 要编码的长度值
 * @return ssize_t 写入字节数，失败返回 -1
 */
ssize_t rdb_save_len(RedisModuleIO *io, uint64_t len) {
    unsigned char buf[2];

    if (len < (1<<6)) {
        /* 6 位编码：1 字节 */
        buf[0] = (len&0xFF)|(0<<6);
        if (rdb_write_raw(io, buf, 1) == -1) return -1;
        return 1;
    } else if (len < (1<<14)) {
        /* 14 位编码：2 字节 */
        buf[0] = ((len>>8)&0xFF)|(1<<6);
        buf[1] = len&0xFF;
        if (rdb_write_raw(io, buf, 2) == -1) return -1;
        return 2;
    } else if (len <= UINT32_MAX) {
        /* 32 位编码：标志字节 + 4 字节网络序整数 */
        buf[0] = (2<<6);
        if (rdb_write_raw(io, buf, 1) == -1) return -1;
        uint32_t len32 = htonl(len);
        if (rdb_write_raw(io, &len32, 4) == -1) return -1;
        return 1+4;
    } else {
        /* 64 位编码：标志字节 0x81 + 8 字节网络序整数 */
        buf[0] = (3<<6)|0x01;
        if (rdb_write_raw(io, buf, 1) == -1) return -1;
        uint64_t len64 = htonll(len);
        if (rdb_write_raw(io, &len64, 8) == -1) return -1;
        return 1+8;
    }
}

/**
 * @brief 将 64 位整数转换为网络字节序（大端）
 * 在小端系统上逐字节翻转，在大端系统上直接返回。
 * @param v 主机字节序的 64 位整数
 * @return uint64_t 网络字节序的 64 位整数
 */
#ifndef htonll
uint64_t htonll(uint64_t v) {
    int num = 1;
    if(*(char *)&num == 1) { /* 小端系统：需要翻转字节 */
        union { uint64_t u; uint8_t c[8]; } t = { v };
        return ((uint64_t)t.c[0] << 56) | ((uint64_t)t.c[1] << 48) |
               ((uint64_t)t.c[2] << 40) | ((uint64_t)t.c[3] << 32) |
               ((uint64_t)t.c[4] << 24) | ((uint64_t)t.c[5] << 16) |
               ((uint64_t)t.c[6] << 8)  | ((uint64_t)t.c[7]);
    } else {
        return v; /* 大端系统：无需翻转 */
    }
}
#endif

/**
 * @brief 将网络字节序 64 位整数转换为主机字节序（htonll 的逆运算）
 * @param v 网络字节序的 64 位整数
 * @return uint64_t 主机字节序的 64 位整数
 */
#ifndef ntohll
uint64_t ntohll(uint64_t v) {
    return htonll(v); /* htonll 是自对称的，直接复用 */
}
#endif

/**
 * @brief 读取变长长度编码整数
 *
 * 解码规则（与 rdb_save_len 对应）：
 *   - 高 2 位 == 00：6 位直接值
 *   - 高 2 位 == 01：14 位值（读 1 个额外字节）
 *   - 高 2 位 == 10：32 位值（读 4 个额外字节，网络序）
 *   - 高 2 位 == 11：特殊编码，isencoded 置 1
 *
 * @param io        IO 上下文
 * @param isencoded 输出参数：非 0 表示该值是编码整数而非长度
 * @return uint64_t 解码后的长度值，失败返回 RDB_LENERR
 */
uint64_t rdb_load_len(RedisModuleIO *io, int *isencoded) {
    unsigned char buf[2];
    if (isencoded) *isencoded = 0;

    if (rdb_read_raw(io, buf, 1) == -1) return RDB_LENERR;
    int type = (buf[0]&0xC0)>>6; /* 取高 2 位判断编码类型 */

    if (type == 0) {
        return buf[0]&0x3F;  /* 6 位直接值 */
    } else if (type == 1) {
        /* 14 位值：读取第 2 字节，拼接为 14 位整数 */
        if (rdb_read_raw(io, buf+1, 1) == -1) return RDB_LENERR;
        return ((buf[0]&0x3F)<<8)|buf[1];
    } else if (type == 2) {
        /* 32 位值：读取 4 字节，网络序转主机序 */
        uint32_t len;
        if (rdb_read_raw(io, &len, 4) == -1) return RDB_LENERR;
        return ntohl(len);
    } else if (type == 3) {
        /* 特殊编码：标记 isencoded，返回低 6 位编码标识 */
        if (isencoded) *isencoded = 1;
        return buf[0]&0x3F;
    }
    return RDB_LENERR;
}

/**
 * @brief 写入字符串（长度前缀 + 内容）
 * 先写入变长长度，再写入原始字符串数据。
 * @param io  IO 上下文
 * @param str 字符串数据
 * @param len 字符串长度
 * @return ssize_t 总写入字节数，失败返回 -1
 */
ssize_t rdb_save_string(RedisModuleIO *io, const char *str, size_t len) {
    ssize_t n1 = rdb_save_len(io, len); /* 写长度前缀 */
    if (n1 == -1) return -1;
    ssize_t n2 = 0;
    if (len > 0) {
        n2 = rdb_write_raw(io, str, len); /* 写字符串内容 */
        if (n2 == -1) return -1;
    }
    return n1 + n2;
}

/**
 * @brief 读取字符串（长度前缀 + 内容）
 * 读取长度前缀后分配 SDS，再读取内容。
 * 暂不支持特殊编码（LZF 压缩/整数编码），遇到时返回 NULL。
 * @param io IO 上下文
 * @return sds 读取到的 SDS 字符串，失败返回 NULL
 */
sds rdb_load_string(RedisModuleIO *io) {
    int isencoded;
    uint64_t len = rdb_load_len(io, &isencoded);
    if (isencoded) {
        /* TODO: 支持整数编码和 LZF 压缩字符串 */
        return NULL;
    }
    if (len == RDB_LENERR) return NULL;

    sds val = sds_new_len(NULL, len); /* 预分配 SDS 空间 */
    if (len > 0 && rdb_read_raw(io, val, len) == -1) {
        sds_delete(val);
        return NULL;
    }
    return val;
}

/* ==================== 对象序列化 ==================== */

/**
 * @brief 序列化并写入一个 latte_object_t 对象
 * 当前支持 OBJ_STRING（写入字符串内容）和 OBJ_MODULE（调用模块的 save 回调）。
 * 其他类型返回 -1（待实现）。
 * @param io IO 上下文
 * @param o  要保存的对象指针
 * @return ssize_t 写入字节数，不支持的类型或失败返回 -1
 */
ssize_t rdb_save_object(RedisModuleIO *io, latte_object_t *o) {
    ssize_t n = 0;

    if (o->type == OBJ_STRING) {
        /* 字符串对象：直接写入 SDS 内容 */
        n = rdb_save_string(io, o->ptr, sds_len(o->ptr));
    } else if (o->type == OBJ_MODULE) {
        /* 模块对象：通过模块类型的 save 回调序列化 */
        module_value_t *mv = o->ptr;
        module_type_t *mt = mv->type;
        /* TODO: 写入模块 ID（目前假设模块类型已知或固定） */
        if (mt->save) {
            mt->save((void*)io, mv->value);
        }
    } else {
        /* TODO: 支持 list/hash/set/zset 等类型 */
        return -1;
    }
    return n;
}

/**
 * @brief 从 RDB 中加载指定类型的对象
 * 当前仅支持 OBJ_STRING。OBJ_MODULE 需要模块注册表支持（依赖 server 层）。
 * @param io   IO 上下文
 * @param type 对象类型（由 rdb_load_type 读取）
 * @return latte_object_t* 加载的对象，失败或不支持的类型返回 NULL
 */
latte_object_t *rdb_load_object(RedisModuleIO *io, int type) {
    if (type == OBJ_STRING) {
        sds s = rdb_load_string(io);
        if (!s) return NULL;
        return latte_object_new(OBJ_STRING, s);
    }
    /* OBJ_MODULE：需要通过服务器模块注册表查找模块类型后才能加载，
     * 此处暂不实现，返回 NULL */
    return NULL;
}
