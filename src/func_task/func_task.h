/**
 * @file func_task.h
 * @brief 函数任务封装模块
 *        将任意函数及其参数打包为一个可调度的任务对象
 *        支持执行函数（exec）和完成回调（cb），兼容线程池投递
 */

#ifndef __FUNC_TASK_H
#define __FUNC_TASK_H

/** 前向声明，避免循环引用 */
typedef struct latte_func_task_t latte_func_task_t;

/**
 * @brief 任务执行函数类型
 * @param task 当前任务对象，可从 task->args 获取参数，结果写入 task->result
 * @return 0 表示成功，非 0 表示错误码
 */
typedef int task_fn(latte_func_task_t* task);

/**
 * @brief 任务完成回调函数类型
 * @param task 已完成的任务对象（可读取 result / error）
 */
typedef void callback_fn(latte_func_task_t* task);

/**
 * @brief 函数任务结构体
 *        封装一次函数调用所需的全部信息：参数列表、执行函数、回调及结果
 */
typedef struct latte_func_task_t {
    int argv;           /**< 参数个数 */
    void** args;        /**< 参数指针数组，长度为 argv */
    void* result;       /**< 任务执行结果，由 exec 函数写入 */
    int error;          /**< 错误码，0 表示成功 */
    int id;             /**< 任务 ID（可选标识） */
    task_fn *exec;      /**< 执行函数指针 */
    callback_fn *cb;    /**< 完成回调函数指针，可为 NULL */
} latte_func_task_t;

/**
 * @brief 创建一个函数任务对象
 *        使用可变参数传递 argv 个参数指针，依次存入 args 数组
 * @param exec 任务执行函数
 * @param cb   任务完成回调（可为 NULL）
 * @param argv 参数个数
 * @param ...  各参数指针（void* 类型），共 argv 个
 * @return 新建的 latte_func_task_t 指针
 */
latte_func_task_t* latte_func_task_new(task_fn* exec, callback_fn* cb, int argv, ...);

/**
 * @brief 执行任务：调用 task->exec(task)
 * @param task 要执行的任务
 */
void exec_task(latte_func_task_t* task);

/**
 * @brief 触发任务回调：若 cb 不为 NULL 则调用 task->cb(task)
 * @param task 已完成的任务
 */
void callback_task(latte_func_task_t* task);

/**
 * @brief 销毁函数任务对象，释放 args 数组及任务结构体本身
 * @param task 要释放的任务对象
 */
void latte_func_task_delete(latte_func_task_t* task);


#endif
