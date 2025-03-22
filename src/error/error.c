#include "error.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
int error_is_not_found(latte_error_t* error) {
    return error->code == CNotFound;
}
int error_is_ok(latte_error_t* error) {
    return error->code == COk;
}

//只有在error不是ok的情况下创建error
latte_error_t* error_new(latte_error_code_enum code, char* context, char* message, ...) {
    latte_error_t* error = (latte_error_t*)zmalloc(sizeof(latte_error_t));
    error->code = code;
    error->state = sds_cat_printf(sds_empty(),"%s: %s",context, message);;
    return error;
}

latte_error_t* errno_io_new(char* format, ...) {
    latte_error_code_enum code;
    if (errno == ENOENT) {
        code = CNotFound; 
    } else {
        code = CIOError; 
    }
    va_list args;
    

    va_start(args, format); // 初始化va_list
    char* buf = NULL;

    char cache[512];
    buf = cache;
    int need_free = 0;
    size_t result = vsnprintf(buf, sizeof(cache), format, args);
    if (result >= sizeof(buf)) {
        buf = zmalloc(sizeof(result + 1));
        vsnprintf(cache, result + 1, format, args);
        need_free = 1;
    }
    va_end(args); // 清理va_list
    latte_error_t* err = error_new(code, buf, strerror(errno));
    if (need_free) zfree(buf);
    return err;
}

void error_delete(latte_error_t* error) {
    if (error->state != NULL) {
        sds_delete(error->state);
    }
    zfree(error);
}

latte_error_t* io_error_new(char* context, char* message) {
    return error_new(CIOError, context, message);
}