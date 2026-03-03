/**
 * odb 模块：基于 oio 的序列化 I/O，支持字符串、二进制、数字的写入与读取。
 * 依赖 oio 后端（内存 buffer、文件等）。
 */
#ifndef __LATTE_ODB_H__
#define __LATTE_ODB_H__

#include "odb/oio.h"
#include <stdint.h>
#include <stddef.h>

/* 项目内 sds 类型（oio.h 已依赖 sds） */
#include "sds/sds.h"

/* ---------- oio 创建与释放 ---------- */

/** 创建基于内存缓冲的 oio，初始为空；调用方负责 oio_free */
oio *odb_oio_create_buffer(void);

/** 创建基于 FILE* 的 oio，不拥有 fp（不 fclose）；调用方负责 oio_free */
oio *odb_oio_create_file(FILE *fp);

/** 释放 oio 及其后端资源（buffer 会释放 sds，file 不关闭 fp） */
void odb_oio_free(oio *o);

/** 仅 buffer 后端：将读位置重置到开头，便于写完后读回 */
void odb_oio_buffer_rewind(oio *o);

/* ---------- 字符串：先写/读长度(uint32_t)，再写/读字节（不含结尾 \\0） ---------- */

/** 写入：4 字节长度(小端) + 数据；len 为字节数 */
int odb_write_string(oio *o, const void *s, size_t len);

/** 读取：先读长度再读数据，返回 sds（调用方 sds_delete）；失败返回 NULL */
sds odb_read_string(oio *o);

/* ---------- 二进制：同字符串，先长度后数据 ---------- */

/** 写入：4 字节长度(小端) + 数据 */
int odb_write_binary(oio *o, const void *buf, size_t len);

/** 读取：返回 malloc 的缓冲区及长度，调用方 free(*out)；失败返回 -1，成功返回 0 */
int odb_read_binary(oio *o, void **out, size_t *out_len);

/* ---------- 数字：定长小端 ---------- */

int odb_write_u8(oio *o, uint8_t v);
int odb_write_u16(oio *o, uint16_t v);
int odb_write_u32(oio *o, uint32_t v);
int odb_write_u64(oio *o, uint64_t v);

int odb_read_u8(oio *o, uint8_t *v);
int odb_read_u16(oio *o, uint16_t *v);
int odb_read_u32(oio *o, uint32_t *v);
int odb_read_u64(oio *o, uint64_t *v);

#endif /* __LATTE_ODB_H__ */
