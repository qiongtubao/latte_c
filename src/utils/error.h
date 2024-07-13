#ifndef __LATTE_ERROR_H
#define __LATTE_ERROR_H

#include "sds/sds.h"
#include "zmalloc/zmalloc.h"
#include <errno.h>    // 包含errno和错误码
#include <dirent.h>
#include "utils/error.h"
typedef enum Code {
    COk = 0, //操作正常
    CNotFound = 1, //没找到相关项
    CCorruption = 2, //异常崩溃
    CNotSupported = 3, //不支持
    CInvalidArgument = 4, //非法参数
    CIOError = 5 //I/O操作错误
} Code;

typedef struct Error {
    Code code;
    sds state;
} Error;

static Error Ok = {COk, NULL};

Error* errnoIoCreate(char* context, ...);
Error* errorCreate(Code code, char* state, char* message, ...);
void errorRelease(Error* error);
int isOk(Error* error);
int isNotFound(Error* error);
Error* ioErrorCreate(char* state, char* message);


#endif