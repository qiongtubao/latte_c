/**
 * @file fs.c
 * @brief 文件系统工具函数实现
 *        提供文件存在检查、循环读写（处理 EINTR/EAGAIN）、全量读取等基础操作
 */
#include "fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <error.h>
#include <errno.h>
#include <sys/stat.h>

/**
 * @brief 检查文件是否存在
 *        通过 access(F_OK) 系统调用判断文件可访问性
 * @param filename 文件路径字符串
 * @return true 文件存在；false 不存在
 */
bool file_exists(char* filename) {
  return access(filename, F_OK) == 0;
}

/**
 * @brief 向 fd 写入恰好 size 字节，自动循环处理短写及 EINTR/EAGAIN
 *        适用于普通文件写入和 socket 发送场景
 * @param fd   目标文件描述符
 * @param buf  待写入数据缓冲区
 * @param size 要写入的总字节数
 * @return 0 写入完成；非 0 为系统 errno 错误码（非 EINTR/EAGAIN 时返回）
 */
int writen(int fd, const void* buf, int size) {
  const char* tmp = (const char*) buf;
  while (size > 0) {
    const ssize_t ret = write(fd, tmp, size);
    if (ret >= 0) {
      tmp += ret;
      size -= ret;
      continue;
    }
    /* 忽略 EAGAIN/EINTR，其他错误立即返回 */
    const int err = errno;
    if (EAGAIN != err && EINTR != err)
      return err;
  }
  return 0;
}

/**
 * @brief 从 fd 读取恰好 size 字节，自动循环处理短读及 EINTR/EAGAIN
 *        适用于普通文件读取和 socket 接收场景
 * @param fd   源文件描述符
 * @param buf  接收数据缓冲区
 * @param size 要读取的总字节数
 * @param len  实际已读字节数的累加输出（可为 NULL）
 * @return  0 读满 size 字节；
 *         -1 文件已到末尾（read 返回 0）；
 *         非 0 为系统 errno 错误码
 */
int readn(int fd, void *buf, int size, int* len) {
  char *tmp = (char *)buf;
  while (size > 0) {
    const ssize_t ret = read(fd, tmp, size);
    if (ret > 0) {
      tmp += ret;
      size -= ret;
      if (len != NULL) *len += ret;
      continue;
    }
    if (0 == ret) {
      return -1;  /* 文件结束（EOF） */
    }
    /* 忽略 EAGAIN/EINTR，其他错误立即返回 */
    const int err = errno;
    if (EAGAIN != err && EINTR != err)
      return err;
  }
  return 0;
}

/**
 * @brief 一次性读取 fd 的全部内容，以 sds 字符串返回
 *        通过 fstat 获取文件大小后分配缓冲区，调用 readn 完整读取
 * @param fd 已打开的文件描述符（-1 时打印错误并返回 NULL）
 * @return 成功返回包含全部文件内容的 sds 字符串；
 *         失败（fd 无效/fstat 失败/内存不足/读取不完整）返回 NULL
 */
sds_t readall(int fd) {
  if (fd == -1) {
    perror("Error opening file");
    return NULL;
  }
  struct stat sb;
  if (fstat(fd, &sb) == -1) {
      perror("Error getting file size");
      close(fd);
      return NULL;
  }
  int len = sb.st_size;
  sds_t buffer = sds_new_len("", len);
  if (buffer == NULL) {
    perror("Error allocating memory");
    return NULL;
  }
  int size = 0;
  /* 读取文件内容到 sds 缓冲区 */
  int error = readn(fd, buffer, len, &size);
  if (error != 0 || size != len) {
      perror("Error reading file");
      sds_delete(buffer);
      return NULL;
  }
  return buffer;
}
