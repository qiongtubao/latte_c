#include <stdio.h>
#include <sys/stat.h> // 包含mkdir函数声明
#include <errno.h>    // 包含errno和错误码
#include <dirent.h>
#include <fcntl.h>
#include <string.h>

#include "dir.h"

Error* dirCreate(char* path) {
    // 创建目录
    int status = mkdir(path, 0755);
    // 检查mkdir函数的返回值
    if (status != 0) {
        if (errno == EEXIST) {
            //已经存在了，不报错
            return &Ok;
        } 
        return errnoIoCreate("create dir fail");
    }
    return &Ok;
}

Error* dirCreateRecursive(const char* path, mode_t mode) {
    char *buf;
    size_t len;
    int status = 0;
    int i;

    buf = strdup(path);
    if (!buf) {
        perror("strdup");
        return -1;
    }

    len = strlen(buf) + 1;
    for (i = 1; i < len; ++i) {
        if (buf[i] == '/' || buf[i] == '\\') {
            buf[i] = '\0';
            if (mkdir(buf, mode) == -1 && errno != EEXIST) {
                perror("mkdir");
                status = -1;
                break;
            }
            buf[i] = '/';
        }
    }

    free(buf);
    return status;
}

int dirIs(char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        // 如果路径不存在或无法访问，返回错误
        return 0;
    }
    // S_ISDIR 宏检查是否为目录
    return S_ISDIR(st.st_mode);
}