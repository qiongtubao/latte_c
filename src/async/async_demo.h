/**
 * @file async_demo.h
 * @brief 异步任务演示接口（Demo）
 *        定义任务检查、回调、执行三种函数类型，以及基础任务结构体（latteTask）
 *        和串行任务结构体（seriesTask），用于演示异步任务调度框架的基本用法。
 */

#include "sds/sds.h"
#include "dict/dict.h"
#include "task/task.h"

/**
 * @brief 任务检查回调函数类型
 *        用于在执行前判断任务是否满足执行条件。
 * @param task 目标任务指针
 * @return 满足条件返回非零；否则返回 0
 */
typedef int latteTaskCheck(latteTask* task);

/**
 * @brief 任务完成回调函数类型
 *        任务执行完成后调用，用于通知上层处理结果。
 * @param task 目标任务指针
 */
typedef void latteTaskCallback(latteTask* task);

/**
 * @brief 任务执行函数类型
 *        实际执行任务逻辑的回调。
 * @param task 目标任务指针
 */
typedef void latteTaskExec(latteTask* task);

/**
 * @brief 基础异步任务结构体
 *        描述单个异步任务的类型、状态、父任务引用及完成回调。
 */
typedef struct latteTask {
    int type;                /**< 任务类型标识 */
    int status;              /**< 任务当前状态（如 pending/running/done） */
    latteTask* parent;       /**< 父任务指针（NULL 表示顶层任务） */
    latteTaskCallback* cb;   /**< 任务完成回调函数指针 */
} latteTask;

/**
 * @brief 串行任务结构体
 *        继承自 latteTask，用于描述需要顺序执行的一组子任务。
 */
typedef struct seriesTask {
    latteTask task; /**< 基础任务（继承字段） */
} seriesTask;


