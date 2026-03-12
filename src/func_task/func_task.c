/**
 * @file func_task.c
 * @brief 函数任务封装模块实现
 *        通过可变参数创建任务，支持执行和回调两阶段调用
 */
#include "func_task.h"
#include <stdarg.h>
#include "zmalloc/zmalloc.h"

/**
 * @brief 创建函数任务对象
 *        使用可变参数依次填充 args 数组
 * @param exec 任务执行函数
 * @param cb   完成回调（可为 NULL）
 * @param argv 参数个数
 * @param ...  参数指针列表（void*），共 argv 个
 * @return 新建的 latte_func_task_t 指针
 */
latte_func_task_t* latte_func_task_new(task_fn* exec, callback_fn* cb, int argv, ...) {
    va_list valist;
    /* 分配任务结构体及参数指针数组 */
    latte_func_task_t *task = zmalloc(sizeof(latte_func_task_t));
    task->exec = exec;
    task->cb = cb;
    task->argv = argv;
    task->args = zmalloc(sizeof(void*) * argv);
    /* 依次提取可变参数并存入 args 数组 */
    va_start(valist, argv);
    for (int i = 0; i < argv; i++) {
        task->args[i] = va_arg(valist, void *);
    }
    va_end(valist);
    return task;
}

/**
 * @brief 执行任务：直接调用 task->exec(task)
 * @param task 要执行的任务对象
 */
void exec_task(latte_func_task_t* task) {
    task->exec(task);
}

/**
 * @brief 触发任务完成回调
 *        若 cb 为 NULL 则静默跳过
 * @param task 已完成的任务对象
 */
void callback_task(latte_func_task_t* task) {
    if (task->cb) task->cb(task);
}

/**
 * @brief 销毁函数任务对象
 *        先释放 args 数组，再释放任务结构体本身
 * @param task 要释放的任务对象
 */
void latte_func_task_delete(latte_func_task_t* task) {
    if (task->args != NULL) {
        zfree(task->args);
        task->args = NULL;
    }
    zfree(task);
}
