#include "fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <error.h>
#include <errno.h>

/**
  *  写入文件或者socket
  *  return  0 写入完成
  *  其他为错误 
  *   
  *
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
    const int err = errno;
    if (EAGAIN != err && EINTR != err)
      return err;
  }
  return 0;
}

/**
  * 读取文件或者socket
  *  return  0 读满了buffer
  *  return -1 文件结束
  *  其他为错误 
  *   
  *
*/
int readn(int fd, void *buf, int size, int* len) {
  char *tmp = (char *)buf;
  int l = 0;
  while (size > 0) {
    const ssize_t ret = read(fd, tmp, size);
    if (ret > 0) {
      tmp += ret;
      size -= ret;
      if (len != NULL) *len += ret;
      continue;
    }
    if (0 == ret) {
      return -1;  // end of file
    }

    const int err = errno;
    if (EAGAIN != err && EINTR != err)
      return err;
  }
  return 0;
}