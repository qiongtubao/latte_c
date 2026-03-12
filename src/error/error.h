/**
 * @file error.h
 * @brief 错误码与错误对象接口
 *        定义统一的错误枚举、错误结构体及创建/判断/释放函数，
 *        用于在函数调用链中传递和检查操作结果。
 */
#ifndef __LATTE_ERROR_H
#define __LATTE_ERROR_H


#include "sds/sds.h"
#include "zmalloc/zmalloc.h"
#include <errno.h>    // 包含errno和错误码
#include <dirent.h>

/**
 * @brief 错误码枚举
 *        表示各类操作的结果状态，可根据需要继续扩展新的错误类型。
 */
typedef enum latte_error_code_enum {
    COk = 0,              /**< 操作正常，无错误 */
    CNotFound = 1,        /**< 未找到相关项 */
    CCorruption = 2,      /**< 数据异常或内部状态损坏 */
    CNotSupported = 3,    /**< 操作不受支持 */
    CInvalidArgument = 4, /**< 非法参数 */
    CIOError = 5          /**< I/O 操作错误；其他类型错误可继续在此后扩展 */
} latte_error_code_enum;

/**
 * @brief 错误对象结构体
 *        封装错误码与详细描述字符串，用于函数返回错误信息。
 */
typedef struct latte_error_t {
    latte_error_code_enum code; /**< 错误码，标识错误类型 */
    sds_t state;                /**< 错误详细描述字符串（COk 时为 NULL） */
} latte_error_t;

/**
 * @brief 预定义的"无错误"静态对象
 *        用于直接返回成功状态，无需动态分配内存，调用方不可释放此对象。
 */
static latte_error_t Ok = {COk, NULL};

/**
 * @brief 根据当前 errno 创建 I/O 错误对象
 *        自动读取全局 errno 并将其格式化为错误描述附加到 context 中。
 * @param context printf 格式化的上下文描述字符串
 * @param ...     与 context 对应的可变参数
 * @return 新建的 latte_error_t 指针（CIOError）；内存分配失败返回 NULL
 */
latte_error_t* errno_io_new(char* context, ...);

/**
 * @brief 创建任意类型的错误对象
 * @param code    错误码
 * @param state   错误状态标签字符串（如模块名或操作名）
 * @param message printf 格式化的详细错误描述
 * @param ...     与 message 对应的可变参数
 * @return 新建的 latte_error_t 指针；内存分配失败返回 NULL
 */
latte_error_t* error_new(latte_error_code_enum code, char* state, char* message, ...);

/**
 * @brief 释放错误对象内存
 *        释放 state 字符串及错误对象本身；传入静态 Ok 对象时行为未定义，
 *        应仅对 error_new/errno_io_new/io_error_new 返回的指针调用。
 * @param error 要释放的错误对象指针（NULL 时安全跳过）
 */
void error_delete(latte_error_t* error);

/**
 * @brief 判断错误对象是否表示成功（COk）
 * @param error 错误对象指针（NULL 视为非 Ok）
 * @return error 为 NULL 或 code == COk 返回 1（真）；否则返回 0（假）
 */
int error_is_ok(latte_error_t* error);

/**
 * @brief 判断错误对象是否表示"未找到"（CNotFound）
 * @param error 错误对象指针
 * @return code == CNotFound 返回 1（真）；否则返回 0（假）
 */
int error_is_not_found(latte_error_t* error);

/**
 * @brief 创建 I/O 错误对象（手动指定描述）
 *        与 errno_io_new 不同，此函数不读取 errno，而是直接使用传入的消息。
 * @param state   错误状态标签字符串（如文件路径或模块名）
 * @param message 错误详细描述字符串
 * @return 新建的 latte_error_t 指针（CIOError）；内存分配失败返回 NULL
 */
latte_error_t* io_error_new(char* state, char* message);



#endif
