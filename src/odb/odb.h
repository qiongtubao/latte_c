/**
 * @file odb.h
 * @brief 基于 oio 的序列化 I/O 模块接口
 *        在 oio 通用 I/O 抽象之上封装字符串、二进制、定长数字的读写操作，
 *        数字统一使用小端字节序，字符串/二进制先写 4 字节长度再写数据。
 */
#ifndef __LATTE_ODB_H__
#define __LATTE_ODB_H__

#include "odb/oio.h"
#include <stdint.h>
#include <stddef.h>

/* 项目内 sds 类型（oio.h 已依赖 sds） */
#include "sds/sds.h"

/* ---------- oio 创建与释放 ---------- */

/**
 * @brief 创建基于内存缓冲的 oio
 *        初始缓冲为空，适用于序列化/反序列化场景；调用方负责 odb_oio_free
 * @return 新建的 oio 指针；失败返回 NULL
 */
oio *odb_oio_create_buffer(void);

/**
 * @brief 创建基于 FILE* 的 oio
 *        不拥有 fp（不执行 fclose）；调用方负责 odb_oio_free
 * @param fp 目标文件指针
 * @return 新建的 oio 指针；失败返回 NULL
 */
oio *odb_oio_create_file(FILE *fp);

/**
 * @brief 释放 oio 及其后端资源
 *        buffer 后端会释放 sds 缓冲区；file 后端不关闭 fp
 * @param o 要释放的 oio 指针
 */
void odb_oio_free(oio *o);

/**
 * @brief 将 buffer 后端的读位置重置到开头
 *        仅适用于 buffer 后端，便于写入完成后从头读取
 * @param o 目标 buffer oio 指针
 */
void odb_oio_buffer_rewind(oio *o);

/* ---------- 字符串：先写/读 uint32_t 长度（小端），再写/读字节（不含结尾 \0） ---------- */

/**
 * @brief 写入字符串（4 字节小端长度 + 数据字节）
 * @param o   目标 oio
 * @param s   字符串数据指针
 * @param len 字符串字节数（不含 \0）
 * @return 成功返回 0；失败返回 -1
 */
int odb_write_string(oio *o, const void *s, size_t len);

/**
 * @brief 读取字符串（先读 4 字节长度，再读数据）
 * @param o 源 oio
 * @return 成功返回 sds 字符串（调用方负责 sds_delete 释放）；失败返回 NULL
 */
sds odb_read_string(oio *o);

/* ---------- 二进制：先写/读 uint32_t 长度（小端），再写/读数据 ---------- */

/**
 * @brief 写入二进制数据（4 字节小端长度 + 数据）
 * @param o   目标 oio
 * @param buf 数据缓冲区指针
 * @param len 数据字节数
 * @return 成功返回 0；失败返回 -1
 */
int odb_write_binary(oio *o, const void *buf, size_t len);

/**
 * @brief 读取二进制数据（先读 4 字节长度，再读数据）
 * @param o       源 oio
 * @param out     输出：malloc 分配的数据缓冲区指针（调用方负责 free(*out)）
 * @param out_len 输出：数据字节数
 * @return 成功返回 0；失败返回 -1
 */
int odb_read_binary(oio *o, void **out, size_t *out_len);

/* ---------- 定长数字：小端字节序 ---------- */

/**
 * @brief 写入 1 字节无符号整数
 * @param o 目标 oio
 * @param v 要写入的值
 * @return 成功返回 0；失败返回 -1
 */
int odb_write_u8(oio *o, uint8_t v);

/**
 * @brief 写入 2 字节小端无符号整数
 * @param o 目标 oio
 * @param v 要写入的值
 * @return 成功返回 0；失败返回 -1
 */
int odb_write_u16(oio *o, uint16_t v);

/**
 * @brief 写入 4 字节小端无符号整数
 * @param o 目标 oio
 * @param v 要写入的值
 * @return 成功返回 0；失败返回 -1
 */
int odb_write_u32(oio *o, uint32_t v);

/**
 * @brief 写入 8 字节小端无符号整数
 * @param o 目标 oio
 * @param v 要写入的值
 * @return 成功返回 0；失败返回 -1
 */
int odb_write_u64(oio *o, uint64_t v);

/**
 * @brief 读取 1 字节无符号整数
 * @param o 源 oio
 * @param v 输出：读取的值
 * @return 成功返回 0；失败返回 -1
 */
int odb_read_u8(oio *o, uint8_t *v);

/**
 * @brief 读取 2 字节小端无符号整数
 * @param o 源 oio
 * @param v 输出：读取的值
 * @return 成功返回 0；失败返回 -1
 */
int odb_read_u16(oio *o, uint16_t *v);

/**
 * @brief 读取 4 字节小端无符号整数
 * @param o 源 oio
 * @param v 输出：读取的值
 * @return 成功返回 0；失败返回 -1
 */
int odb_read_u32(oio *o, uint32_t *v);

/**
 * @brief 读取 8 字节小端无符号整数
 * @param o 源 oio
 * @param v 输出：读取的值
 * @return 成功返回 0；失败返回 -1
 */
int odb_read_u64(oio *o, uint64_t *v);

#endif /* __LATTE_ODB_H__ */
