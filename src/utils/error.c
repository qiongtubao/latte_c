#include "error.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
int isNotFound(Error* error) {
    return error->code == CNotFound;
}
int isOk(Error* error) {
    return error->code == COk;
}
//只有在error不是ok的情况下创建error
Error* errorCreate(Code code, char* context, char* message, ...) {
    Error* error = (Error*)zmalloc(sizeof(Error));
    error->code = code;
    error->state = sds_cat_printf(sds_empty(),"%s: %s",context, message);;
    return error;
}

Error* errnoIoCreate(char* format, ...) {
    Code code;
    if (errno == ENOENT) {
        code = CNotFound; 
    } else {
        code = CIOError; 
    }
    va_list args;
    

    va_start(args, format); // 初始化va_list
    char buf[512];
    int result = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args); // 清理va_list
    return errorCreate(code, buf, strerror(errno));
}

void errorRelease(Error* error) {
    if (error->state != NULL) {
        sds_delete(error->state);
    }
    zfree(error);
}

Error* ioErrorCreate(char* context, char* message) {
    return errorCreate(CIOError, context, message);
}