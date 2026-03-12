/**
 * @file error.c
 * @brief 错误处理实现
 *        提供错误状态检查、错误对象创建和销毁等功能
 */
#include "error.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

/**
 * @brief 检查错误是否为"未找到"类型
 * @param error 错误对象指针
 * @return 1 表示是未找到错误，0 表示不是
 */
int error_is_not_found(latte_error_t* error) {
    return error->code == CNotFound;
}

/**
 * @brief 检查错误状态是否为正常
 * @param error 错误对象指针
 * @return 1 表示正常，0 表示有错误
 */
int error_is_ok(latte_error_t* error) {
    return error->code == COk;
}

/**
 * @brief 创建新的错误对象（仅在错误状态不是 OK 时创建）
 * @param code    错误代码
 * @param context 错误上下文字符串
 * @param message 错误消息格式字符串
 * @param ...     格式化参数
 * @return 新创建的错误对象指针
 */
latte_error_t* error_new(latte_error_code_enum code, char* context, char* message, ...) {
    latte_error_t* error = (latte_error_t*)zmalloc(sizeof(latte_error_t));
    error->code = code;
    error->state = sds_cat_printf(sds_empty(),"%s: %s",context, message);;
    return error;
}

/**
 * @brief 根据 errno 创建 IO 错误对象
 *        自动根据 errno 值确定错误类型（NotFound 或 IOError）
 * @param format 错误消息格式字符串
 * @param ...    格式化参数
 * @return 新创建的错误对象指针
 */
latte_error_t* errno_io_new(char* format, ...) {
    latte_error_code_enum code;
    if (errno == ENOENT) {
        code = CNotFound;
    } else {
        code = CIOError;
    }
    va_list args;


    va_start(args, format); /* 初始化 va_list */
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
    va_end(args); /* 清理 va_list */
    latte_error_t* err = error_new(code, buf, strerror(errno));
    if (need_free) zfree(buf);
    return err;
}

/**
 * @brief 销毁错误对象，释放相关内存
 * @param error 要销毁的错误对象指针
 */
void error_delete(latte_error_t* error) {
    if (error->state != NULL) {
        sds_delete(error->state);
    }
    zfree(error);
}

/**
 * @brief 创建 IO 错误对象
 * @param context 错误上下文字符串
 * @param message 错误消息字符串
 * @return 新创建的 IO 错误对象指针
 */
latte_error_t* io_error_new(char* context, char* message) {
    return error_new(CIOError, context, message);
}