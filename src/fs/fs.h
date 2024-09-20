#ifndef __LATTE_FS_H
#define __LATTE_FS_H

//可用于发送socket 和文件   暂时先把函数放在这里 之后整理再调整
int writen(int fd, const void* buf, int size);
int readn(int fd, void *buf, int size, int* len);
#endif