#ifndef __LATTE_ERROR_H
#define __LATTE_ERROR_H

#include "sds/sds.h"
#include "zmalloc/zmalloc.h"
#include <errno.h>    // 包含errno和错误码
#include <dirent.h>
#include "utils/error.h"
typedef enum latte_code_enum {
    COk = 0, //操作正常
    CNotFound = 1, //没找到相关项
    CCorruption = 2, //异常崩溃
    CNotSupported = 3, //不支持
    CInvalidArgument = 4, //非法参数
    CIOError = 5 //I/O操作错误
} latte_code_enum;

typedef struct latte_error_t {
    latte_code_enum code;
    sds_t state;
} latte_error_t;

static latte_error_t Ok = {COk, NULL};

latte_error_t* errno_io_new(char* context, ...);
latte_error_t* error_new(latte_code_enum code, char* state, char* message, ...);
void error_delete(latte_error_t* error);
int error_is_ok(latte_error_t* error);
int error_is_not_found(latte_error_t* error);
latte_error_t* ioErrorCreate(char* state, char* message);


#endif