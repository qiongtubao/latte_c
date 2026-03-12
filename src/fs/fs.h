/**
 * @file fs.h
 * @brief 文件系统工具函数接口
 *        提供文件存在性检查、全量读写等基础 I/O 操作
 *        注：writen/readn 可用于普通文件也可用于 socket fd
 */
#ifndef __LATTE_FS_H
#define __LATTE_FS_H
#include "error/error.h"
#include <stdbool.h>

/**
 * @brief 检查指定路径的文件是否存在
 * @param path 文件路径字符串
 * @return true 文件存在，false 不存在
 */
bool file_exists(char* path);

/**
 * @brief 向 fd 写入指定字节数，自动处理 EINTR/EAGAIN 重试
 *        可用于普通文件写入或 socket 发送
 * @param fd   目标文件描述符
 * @param buf  待写入数据缓冲区
 * @param size 要写入的字节数
 * @return 0 写入完成；非 0 为系统错误码（errno）
 */
int writen(int fd, const void* buf, int size);

/**
 * @brief 从 fd 读取指定字节数，自动处理 EINTR/EAGAIN 重试
 *        可用于普通文件读取或 socket 接收
 * @param fd   源文件描述符
 * @param buf  接收数据缓冲区
 * @param size 要读取的字节数
 * @param len  实际读取字节数输出（可为 NULL）
 * @return 0 读满 size 字节；-1 文件已到末尾（EOF）；非 0 为系统错误码
 */
int readn(int fd, void *buf, int size, int* len);

/**
 * @brief 一次性读取 fd 所有内容并以 sds 返回
 *        通过 fstat 获取文件大小后一次性分配缓冲区读取
 * @param fd 已打开的文件描述符
 * @return 成功返回包含全部内容的 sds 字符串；失败返回 NULL
 */
sds_t readall(int fd);
#endif
