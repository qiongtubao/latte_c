
/**
 * @file async.h
 * @brief 异步任务调度模块 —— 支持基本任务、串行任务（Series）、并行任务（Parallel）
 *
 * 任务类型：
 *   - asyncBasicTask  : 包装一个 latteThreadJob，提交到线程池执行
 *   - seriesTask      : 串行执行一组子任务，前一个完成后才执行下一个
 *   - parallelTask    : 并行执行多组 seriesTask，所有组完成后整体结束
 *
 * 状态流转：
 *   WAIT_TASK_STATUS → DOING_TASK_STATUS → DONE_TASK_STATUS
 */

#include "sds/sds.h"
#include "dict/dict.h"
#include "task/task.h"

/** @brief 任务类型：单个基础任务 */
#define BASIC_TASK_TYPE    0
/** @brief 任务类型：串行任务组 */
#define SERIES_TASK_TYPE   1
/** @brief 任务类型：并行任务组 */
#define PARALLEL_TASK_TYPE 2


/** @brief 任务状态：等待执行 */
#define WAIT_TASK_STATUS   0
/** @brief 任务状态：正在执行 */
#define DOING_TASK_STATUS  1
/** @brief 任务状态：执行完成 */
#define DONE_TASK_STATUS   2


/**
 * @brief 任务完成回调函数类型
 * @param task 触发回调的任务指针
 */
typedef void asyncCallback(void* task);

/**
 * @brief 任务完成检查函数类型，返回非0表示可以继续
 * @param task 待检查的任务指针
 */
typedef int checkAsyncTask(void* task);

/**
 * @brief 异步任务基类，所有任务类型的公共头部
 */
typedef struct asyncTask {
    int type;               /**< 任务类型：BASIC/SERIES/PARALLEL */
    int status;             /**< 任务状态：WAIT/DOING/DONE */
    struct asyncTask* parent; /**< 父任务指针，根任务为 NULL */
    checkAsyncTask* check;  /**< 可选：完成条件检查函数 */
    asyncCallback* cb;      /**< 可选：任务完成后的回调，NULL 时通知父任务 */
    void* ctx;              /**< 执行上下文（通常存储线程池指针） */
} asyncTask;

/**
 * @brief 基础异步任务，封装一个 latteThreadJob 提交到线程池
 */
typedef struct asyncBasicTask {
    asyncTask task;         /**< 继承自 asyncTask 的公共字段 */
    latteThreadJob* job;    /**< 实际执行的线程作业 */
} asyncBasicTask;

/**
 * @brief 串行任务：依次执行 tasks 列表中的子任务
 */
typedef struct seriesTask {
    /* data */
    asyncTask task;         /**< 继承自 asyncTask 的公共字段 */
    list_t* tasks;          /**< 子任务链表，按顺序依次执行 */
} seriesTask;

/**
 * @brief 并行任务：将多个 seriesTask 组并行调度，所有组完成后结束
 */
typedef struct parallelTask {
  asyncTask task;           /**< 继承自 asyncTask 的公共字段 */
  dict_t* tasks;            /**< 以 sds 名称为 key 的 seriesTask 字典 */
  int num;                  /**< 当前未完成的 seriesTask 组数量 */
} parallelTask;

/**
 * @brief 创建基础异步任务
 * @param job 要执行的线程作业
 * @return 指向新建 asyncTask 的指针（实际类型为 asyncBasicTask）
 */
asyncTask* createAsyncBasicTask(latteThreadJob* job);

/**
 * @brief 调度并运行一个异步任务
 * @param thread 目标线程池
 * @param task   要运行的任务
 */
void asyncRun(taskThread* thread, asyncTask* task);

/**
 * @brief 阻塞等待任务完成（轮询检查状态）
 * @param task 要等待的任务
 */
void asyncWait(asyncTask* task);

/**
 * @brief 获取基础任务的执行结果
 * @param task 已完成的 asyncBasicTask
 * @return 任务结果指针（由 job->result 携带）
 */
void* getBasicTaskResult(asyncBasicTask* task);


/**
 * @brief 创建串行任务容器（初始子任务列表为空）
 * @return 指向新建 asyncTask 的指针（实际类型为 seriesTask）
 */
asyncTask* createSeriesTask();

/**
 * @brief 向串行任务末尾追加一个子任务
 * @param task  目标串行任务
 * @param child 要追加的子任务
 * @return 始终返回 1
 */
int addSeriesTask(seriesTask* task, asyncTask* child);

/**
 * @brief 串行任务内部驱动：当前子任务完成后执行下一个
 * @param current 刚完成的当前子任务
 */
void nextSeriesTask(asyncTask* current);

/**
 * @brief 通知父任务继续（等价于 notifyParent）
 * @param task 已完成的任务
 */
void continueNextTask(asyncTask* task);

/**
 * @brief 创建并行任务容器（初始 seriesTask 组字典为空）
 * @return 指向新建 asyncTask 的指针（实际类型为 parallelTask）
 */
asyncTask* createParallelTask();

/**
 * @brief 向并行任务中指定名称的 seriesTask 组追加子任务
 *        若该名称组不存在则自动创建
 * @param task  目标并行任务
 * @param name  seriesTask 组的名称（sds）
 * @param child 要追加的子任务
 * @return 成功返回 1，任务已完成则返回 0
 */
int addParallelTask(parallelTask* task, sds_t name, asyncTask* child);
