#ifndef __LATTE_FS_H
#define __LATTE_FS_H
#include "utils/error.h"
#include <stdbool.h>

bool file_exists(char* path);
//可用于发送socket 和文件   暂时先把函数放在这里 之后整理再调整
int writen(int fd, const void* buf, int size);
int readn(int fd, void *buf, int size, int* len);
sds readall(int fd);
#endif