/**
 * @file file.h
 * @brief 文件操作接口
 *        提供文件打开/关闭、加锁/解锁、顺序读写等基础文件操作抽象，
 *        支持可写文件（WritableFile）和顺序读文件（SequentialFile）两种模式。
 */
#ifndef __LATTE_FILE_H
#define __LATTE_FILE_H

#include "error/error.h"
#include <stdbool.h>
#include "sds/sds_plugins.h"

/**
 * @brief 打开文件并返回文件描述符
 * @param filename 文件路径字符串
 * @param fd       输出：成功时设置为打开的文件描述符
 * @param flag     open() 标志（如 O_RDWR | O_CREAT）
 * @param mode     创建文件时的权限模式（如 0644）
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* openFile(char* filename, int* fd, int flag, mode_t mode);

/**
 * @brief 对文件描述符加文件锁（flock LOCK_EX）
 * @param fd 文件描述符
 * @return 成功返回 0；失败返回 -1
 */
int lockFile(int fd);

/**
 * @brief 对文件描述符解文件锁（flock LOCK_UN）
 * @param fd 文件描述符
 * @return 成功返回 0；失败返回 -1
 */
int unlockFile(int fd);

/**
 * @brief 文件锁结构体
 *        记录已锁定文件的描述符和文件名，用于后续解锁和释放。
 */
typedef struct FileLock {
    int fd;             /**< 文件描述符 */
    sds_t filename;     /**< 被锁定的文件名（sds 字符串） */
} FileLock;

/**
 * @brief 可写文件结构体（当前为空壳，具体实现由 PosixWritableFile 提供）
 */
typedef struct WritableFile {

} WritableFile;

/**
 * @brief 顺序读文件结构体（当前为空壳，具体实现由 PosixSequentialFile 提供）
 */
typedef struct SequentialFile {

} SequentialFile;

/**
 * @brief 创建文件锁对象
 * @param fd       文件描述符
 * @param filename 文件名字符串（将被复制为 sds）
 * @return 新建的 FileLock 指针；内存分配失败返回 NULL
 */
FileLock* fileLockCreate(int fd, char* filename);

/**
 * @brief 释放文件锁对象（解锁并释放内存，不关闭文件描述符）
 * @param lock 要释放的 FileLock 指针（NULL 时安全跳过）
 */
void fileLockRelease(FileLock* lock);

/**
 * @brief 删除指定文件
 * @param file 文件路径字符串
 * @return 成功返回 0；失败返回 -1
 */
int removeFile(char* file);

/**
 * @brief 关闭文件描述符
 * @param fd 文件描述符
 * @return 成功返回 0；失败返回 -1
 */
int closeFile(int fd);

/**
 * @brief 判断文件是否存在
 * @param filename 文件路径（sds 字符串）
 * @return 存在返回 true；不存在返回 false
 */
bool fileExists(sds_t filename);

/**
 * @brief 重命名文件
 * @param from 原文件路径（sds 字符串）
 * @param to   目标文件路径（sds 字符串）
 * @return 成功返回 0；失败返回 -1
 */
int renameFile(sds_t from, sds_t to);

/* ================== WritableFile ================== */

/**
 * @brief 创建可追加写入的文件对象
 * @param filename 目标文件路径（sds 字符串）
 * @param result   输出：成功时设置为新建的 WritableFile 指针
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* writableFileCreate(sds_t filename,
                         WritableFile** result);

/**
 * @brief 释放可写文件对象（刷新并关闭文件）
 * @param file 要释放的 WritableFile 指针（NULL 时安全跳过）
 */
void writableFileRelease(WritableFile* file);

/**
 * @brief 追加 sds 数据到可写文件
 * @param file 目标 WritableFile 指针
 * @param data 要追加的 sds 数据
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* writableFileAppendSds(WritableFile* file, sds_t data);

/**
 * @brief 追加原始字节数据到可写文件
 * @param file 目标 WritableFile 指针
 * @param buf  数据缓冲区指针
 * @param len  数据长度（字节）
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* writableFileAppend(WritableFile* file, char* buf, size_t len);

/**
 * @brief 追加 slice 数据到可写文件
 * @param file 目标 WritableFile 指针
 * @param data slice_t 数据指针（含指针和长度）
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* writableFileAppendSlice(WritableFile* file, slice_t* data);

/**
 * @brief 刷新可写文件内部缓冲区到操作系统缓存
 * @param file 目标 WritableFile 指针
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* writableFileFlush(WritableFile* file);

/**
 * @brief 将可写文件数据同步到磁盘（调用 fsync/fdatasync）
 * @param file 目标 WritableFile 指针
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* writableFileSync(WritableFile* file);

/**
 * @brief 关闭可写文件（刷新后关闭文件描述符，但不释放 WritableFile 结构体）
 * @param file 目标 WritableFile 指针
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* writableFileClose(WritableFile* file);


/* ================== SequentialFile ================== */

/**
 * @brief 创建顺序读文件对象（打开文件，从头开始顺序读取）
 * @param filename 文件路径（sds 字符串）
 * @param fd       输出：成功时设置为新建的 SequentialFile 指针
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* sequentialFileCreate(sds_t filename, SequentialFile** fd);

/**
 * @brief 从顺序读文件中读取 n 字节
 * @param file  目标 SequentialFile 指针
 * @param n     要读取的字节数
 * @param slice 输出：读取结果（slice_t，含数据指针和实际读取长度）
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* sequentialFileRead(SequentialFile* file, size_t n, slice_t* slice);

/**
 * @brief 从顺序读文件中读取 n 字节到 sds 字符串
 * @param file  目标 SequentialFile 指针
 * @param n     要读取的字节数
 * @param slice 输出：读取结果（sds 指针，调用方负责 sdsfree）
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* sequentialFileReadSds(SequentialFile* file, size_t n, sds* slice);

/**
 * @brief 在顺序读文件中跳过 n 字节（移动读取位置）
 * @param file 目标 SequentialFile 指针
 * @param n    要跳过的字节数
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* sequentialFileSkip(SequentialFile* file, uint64_t n);

/**
 * @brief 释放顺序读文件对象（关闭文件描述符并释放内存）
 * @param file 要释放的 SequentialFile 指针（NULL 时安全跳过）
 */
void sequentialFileRelease(SequentialFile* file);

#endif
