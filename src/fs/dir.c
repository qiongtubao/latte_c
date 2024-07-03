#include <stdio.h>
#include <sys/stat.h> // 包含mkdir函数声明
#include <errno.h>    // 包含errno和错误码
#include <dirent.h>
#include "utils/error.h"
#include "dir.h"

Error* createDir(char* path) {
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