/**
 * @file posix_file.h
 * @brief POSIX 文件操作实现接口
 *        基于 POSIX 系统调用实现可写文件（PosixWritableFile）和顺序读文件
 *        （PosixSequentialFile）两种文件操作对象，提供带缓冲的写入和顺序读取功能。
 */
#ifndef __LATTE_POSIX_FILE_H
#define __LATTE_POSIX_FILE_H

#include "sds/sds.h"
#include <stdbool.h>
#include <stdlib.h>
#include "error/error.h"
#include "sds/sds_plugins.h"

/** @brief 可写文件内部写入缓冲区大小（64KB） */
#define kWritableFileBufferSize  65536


/**
 * @brief POSIX 可写文件结构体
 *        基于内部缓冲区实现高效批量写入，减少系统调用次数。
 *        当缓冲区满或调用 flush/sync 时才将数据写入文件。
 */
typedef struct PosixWritableFile {
    int fd;                             /**< 文件描述符 */
    sds_t filename;                     /**< 文件路径（sds 字符串） */
    sds_t dirname;                      /**< 文件所在目录路径（sds 字符串，用于 sync 目录） */
    size_t pos;                         /**< 内部缓冲区当前写入位置（字节偏移） */
    bool is_manifest;                   /**< 是否为 manifest 文件（影响 sync 行为） */
    char buffer[kWritableFileBufferSize]; /**< 内部写入缓冲区（64KB） */
} PosixWritableFile;

/**
 * @brief 创建 POSIX 可写文件对象
 * @param filename 文件路径字符串（将被复制为 sds）
 * @param fd       已打开的文件描述符
 * @return 新建的 PosixWritableFile 指针；内存分配失败返回 NULL
 */
PosixWritableFile* posixWritableFileCreate(char* filename, int fd);

/**
 * @brief 释放 POSIX 可写文件对象（刷新缓冲区、关闭 fd、释放内存）
 * @param file 要释放的 PosixWritableFile 指针（NULL 时安全跳过）
 */
void posixWritableFileRelease(PosixWritableFile* file);

/**
 * @brief 向 POSIX 可写文件追加数据
 *        数据先写入内部缓冲区，缓冲区满时自动刷新到文件。
 * @param write 目标 PosixWritableFile 指针
 * @param data  要追加的数据指针
 * @param size  数据长度（字节）
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* posixWriteableFileAppend(PosixWritableFile* write, char* data, int size);

/**
 * @brief 将内部缓冲区数据刷新到文件（write 系统调用，不保证落盘）
 * @param writer 目标 PosixWritableFile 指针
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* posixWritableFileFlush(PosixWritableFile* writer);

/**
 * @brief 将文件数据同步到磁盘（调用 fdatasync 或 F_FULLFSYNC）
 *        先刷新缓冲区，再调用系统同步接口确保数据持久化。
 * @param file 目标 PosixWritableFile 指针
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* posixWritableFileSync(PosixWritableFile* file);

/**
 * @brief 关闭 POSIX 可写文件（刷新缓冲区后关闭文件描述符）
 * @param file 目标 PosixWritableFile 指针
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* posixWritableFileClose(PosixWritableFile* file);


/* ============ PosixSequentialFile ============ */

/**
 * @brief POSIX 顺序读文件结构体
 *        封装文件路径和描述符，支持顺序读取（read）和跳过（lseek）操作。
 */
typedef struct PosixSequentialFile {
    sds_t filename; /**< 文件路径（sds 字符串） */
    int fd;         /**< 文件描述符 */
} PosixSequentialFile;

/**
 * @brief 创建 POSIX 顺序读文件对象（打开文件）
 * @param filename 文件路径（sds 字符串）
 * @param file     输出：成功时设置为新建的 PosixSequentialFile 指针
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* posixSequentialFileCreate(sds_t filename, PosixSequentialFile** file);

/**
 * @brief 从 POSIX 顺序读文件中读取 n 字节
 * @param file   目标 PosixSequentialFile 指针
 * @param n      要读取的字节数
 * @param result 输出：读取结果（slice_t，含数据指针和实际读取长度）
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* posixSequentialFileRead(PosixSequentialFile* file, size_t n, slice_t* result);

/**
 * @brief 在 POSIX 顺序读文件中跳过 n 字节（使用 lseek 移动文件偏移）
 * @param file 目标 PosixSequentialFile 指针
 * @param n    要跳过的字节数
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* posixSequentialFileSkip(PosixSequentialFile* file, uint64_t n);

/**
 * @brief 释放 POSIX 顺序读文件对象（关闭 fd 并释放内存）
 * @param file 要释放的 PosixSequentialFile 指针（NULL 时安全跳过）
 */
void posixSequentialFileRelease(PosixSequentialFile* file);
#endif
