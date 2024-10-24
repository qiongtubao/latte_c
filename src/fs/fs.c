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


bool file_exists(char* filename) {
  return access(filename, F_OK) == 0;
}
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
  // int l = 0;
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
  // 读取文件内容
  int error = readn(fd, buffer, len, &size);
  if (error != 0 || size != len) {
      perror("Error reading file");
      sds_delete(buffer);
      return NULL;
  }
  return buffer;
}