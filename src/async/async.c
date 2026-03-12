/**
 * @file async.c
 * @brief 异步任务调度模块实现
 *        支持基础任务（BasicTask）、串行任务（SeriesTask）、并行任务（ParallelTask）
 */
#include "async.h"
#include <assert.h>
#include <string.h>
#include "dict/dict_plugins.h"

/**
 * @brief 创建基础异步任务
 * @param job 要封装的线程作业
 * @return 新建的 asyncTask 指针（实际为 asyncBasicTask）
 */
asyncTask* createAsyncBasicTask(latteThreadJob* job) {
    asyncBasicTask* task = zmalloc(sizeof(asyncBasicTask));
    initTask(task);
    task->task.type = BASIC_TASK_TYPE;
    task->job = job;
    return task;
}

/**
 * @brief 标记任务已完成，触发回调或通知父任务
 *        若 cb != NULL 则调用用户回调；否则通过 notifyParent 驱动父任务继续
 * @param task 已完成的任务
 */
void taskDone(asyncTask* task) {
    task->status = DONE_TASK_STATUS;
    // printf("done: %p\n", task);
    if (task->cb != NULL) {
        task->cb(task);
    } else {
        notifyParent(task);
    }
}

/**
 * @brief 检查任务的完成条件
 *        若 check 为 NULL 则默认认为可执行；否则调用自定义检查函数
 * @param task 要检查的任务
 * @return 1 可执行，0 条件未满足
 */
int checkTask(asyncTask* task) {
    if (task->check == NULL) {
        return 1;
    }
    if (!task->check(task)) {
        return 0;
    }
    return 1;
}

/**
 * @brief BasicTask 执行阶段：调用 job->exec
 * @param job 包含 asyncBasicTask 指针的线程作业（args[0]）
 */
void basicExec(latteThreadJob* job) {
    asyncBasicTask* task = (asyncBasicTask*)job->args[0];
    if (task->job->exec != NULL) {
        task->job->exec(task->job);
    }
}

/**
 * @brief BasicTask 回调阶段：调用 job->cb 后标记任务完成
 * @param job 包含 asyncBasicTask 指针的线程作业（args[0]）
 */
void basicCallback(latteThreadJob* job) {
    asyncBasicTask* task = (asyncBasicTask*)job->args[0];
    if (task->job->cb != NULL) {
        task->job->cb(task->job);
    }
    taskDone(task);
}

/**
 * @brief 运行一个基础任务：将其包装为 latteThreadJob 提交到线程池
 * @param thread 目标线程池
 * @param task   要运行的基础任务
 */
void basicTaskRun(taskThread* thread, asyncTask* task) {
    asyncBasicTask* basic_task= (asyncBasicTask*)task;
    task->status = DOING_TASK_STATUS;
    latteThreadJob* job = createThreadJob(basicExec, basicCallback, 1, task);
    submitTask(thread, job);
}

/**
 * @brief 串行任务驱动：当前子任务完成后，找到其后继子任务并继续执行
 *        若已是最后一个子任务，则通知串行任务整体完成
 * @param task 刚完成的子任务
 */
void nextSeriesTask(asyncTask* task) {
    seriesTask* parent =  task->parent;
    list_node_t* node = list_search_key(parent->tasks, task);
    if (node->next != NULL) {
        asyncTask* nextChild = list_node_value(node->next);
        asyncRun(parent->task.ctx, nextChild);
    } else {
        taskDone(parent);
    }

}

/**
 * @brief 并行任务驱动：某个 seriesTask 组完成时递减计数，全部完成则整体结束
 * @param task 刚完成的 seriesTask 子任务
 */
void nextParallelTask(asyncTask* task) {
    parallelTask* parallel = task->parent;
    parallel->num--;
    if (parallel->num == 0) {
        taskDone(parallel);
    }
}

/**
 * @brief 根据父任务类型分发后继驱动逻辑
 * @param currentTask 刚完成的子任务
 */
void nextTask(asyncTask* currentTask) {
    switch (currentTask->parent->type) {
        case SERIES_TASK_TYPE:
            nextSeriesTask(currentTask);
            break;
        case PARALLEL_TASK_TYPE:
            nextParallelTask(currentTask);
            break;
        default:
            printf("next parent type is not series fail\n");
            break;
    }
}

/**
 * @brief 通知父任务当前子任务已完成，由父任务决定后续行为
 * @param task 已完成的子任务；若无父任务则直接返回
 */
void notifyParent(asyncTask* task) {
    if (task->parent == NULL) return;
    nextTask(task);
}

/**
 * @brief 检查串行任务是否已到达最后一个子任务
 * @param task 当前子任务
 * @return 1 已是最后一个，0 还有后续
 */
int checkSeriesTaskFinished(asyncTask* task) {
    seriesTask* parent = task->parent;
    asyncTask* last = list_node_value(list_last(parent->tasks));
    return last == task;
}

/**
 * @brief 运行串行任务：取第一个子任务开始执行
 *        若子任务列表为空则直接标记完成
 * @param thread 目标线程池
 * @param task   要运行的串行任务
 */
void seriesTaskRun(taskThread* thread, asyncTask* task) {
    seriesTask* series = (seriesTask*)task;
    if (list_length(series->tasks) == 0) {
        taskDone(series);
        return;
    }
    series->task.status = DOING_TASK_STATUS;
    asyncTask* childTask = list_node_value(list_first(series->tasks));
    /* 将线程池指针存入 ctx，供后续子任务驱动时使用 */
    task->ctx = thread;
    asyncRun(thread, childTask);
}

/**
 * @brief 运行并行任务：并发启动所有 seriesTask 组
 *        若组字典为空则直接标记完成
 * @param thread 目标线程池
 * @param task   要运行的并行任务
 */
void parallelTaskRun(taskThread* thread,asyncTask* task) {
    parallelTask* ptask = (parallelTask*)task;
    if (dict_size(ptask->tasks) == 0) {
        taskDone(ptask);
        return;
    }
    ptask->task.status = DOING_TASK_STATUS;
    /* 遍历所有 seriesTask 组并并发启动 */
    latte_iterator_t *di = dict_get_latte_iterator(ptask->tasks);
    while (latte_iterator_has_next(di)) {
        seriesTask* stask = (seriesTask*)latte_pair_value(latte_iterator_next(di));
        seriesTaskRun(thread, stask);
    }
    latte_iterator_delete(di);
    ptask->task.ctx = thread;

}

/**
 * @brief 根据任务类型调度运行：仅对 WAIT 状态的任务生效
 * @param thread 目标线程池
 * @param task   要运行的任务
 */
void asyncRun(taskThread* thread,asyncTask* task) {
    if (task->status != WAIT_TASK_STATUS) return;
    switch (task->type) {
        case BASIC_TASK_TYPE/* constant-expression */:
            /* code */
            basicTaskRun(thread, task);
            break;
        case SERIES_TASK_TYPE:
            seriesTaskRun(thread, task);
            break;
        case PARALLEL_TASK_TYPE:
            parallelTaskRun(thread, task);
            break;
        default:
            break;
    }

}

/**
 * @brief 阻塞等待任务完成（自旋检查 DONE 状态）
 * @param task 要等待的任务
 */
void asyncWait(asyncTask* task) {
    while(1) {
        if (task->status == DONE_TASK_STATUS) {
            return;
        }
        sleep(100);
    }
    return;
}


/**
 * @brief 获取基础任务的执行结果
 * @param task 已完成的 asyncBasicTask
 * @return job->result 指针（由执行函数写入）
 */
void* getBasicTaskResult(asyncBasicTask* task) {
    return task->job->result;
}

/**
 * @brief 初始化 asyncTask 公共字段，状态重置为 WAIT
 * @param task 要初始化的任务指针
 */
void initTask(asyncTask* task) {
    task->status = WAIT_TASK_STATUS;
    task->parent = NULL;
    task->check = NULL;
    task->ctx = NULL;
    task->cb = NULL;
}

/**
 * @brief 创建串行任务容器（子任务列表初始为空）
 * @return 新建的 asyncTask 指针（实际为 seriesTask）
 */
asyncTask* createSeriesTask() {
    seriesTask* task = zmalloc(sizeof(seriesTask));
    initTask(task);
    task->task.type = SERIES_TASK_TYPE;
    task->tasks = list_new();
    return task;
}

/**
 * @brief 向串行任务末尾追加子任务并设置其父指针
 * @param task  目标串行任务
 * @param child 子任务
 * @return 始终返回 1
 */
int addSeriesTask(seriesTask* task, asyncTask* child) {
    list_add_node_tail(task->tasks, child);
    child->parent = task;
    return 1;
}

/**
 * @brief 通知父任务当前任务已完成（continueNextTask 的对外接口）
 * @param task 已完成的任务
 */
void continueNextTask(asyncTask* task) {
    notifyParent(task);
}


/** @brief 并行任务内部字典的 sds key 操作函数表 */
static dict_func_t parallelTaskDictType = {
    dict_sds_hash,                    /* hash function */
    NULL,                           /* key dup */
    NULL,                           /* val dup */
    dict_sds_key_compare,              /* key compare */
    dict_sds_destructor,              /* key destructor */
    NULL,                           /* val destructor */
    NULL                            /* allow to expand */
};

/**
 * @brief 创建并行任务容器（seriesTask 组字典初始为空）
 * @return 新建的 asyncTask 指针（实际为 parallelTask）
 */
asyncTask* createParallelTask() {
    parallelTask* ptask = zmalloc(sizeof(parallelTask));
    initTask(ptask);
    ptask->task.type = PARALLEL_TASK_TYPE;
    ptask->tasks = dict_new(&parallelTaskDictType);
    ptask->num = 0;
    return ptask;
}

/**
 * @brief 向并行任务中指定名称的 seriesTask 组追加子任务
 *        若该名称组尚不存在则自动创建并递增 num
 *        若并行任务已处于 DOING 状态则立即启动新创建的组
 * @param ptask 目标并行任务
 * @param name  seriesTask 组名称（sds）
 * @param child 要追加的子任务
 * @return 成功返回 1；任务已完成返回 0
 */
int addParallelTask(parallelTask* ptask, sds_t name, asyncTask* child) {
    if (ptask->task.status == DONE_TASK_STATUS) {
        return 0;
    }
    dict_entry_t* entry = dict_find(ptask->tasks, name);
    asyncTask* task;
    if (entry == NULL) {
        /* 该名称的 seriesTask 组不存在，创建新组 */
        task = createSeriesTask();
        task->parent = ptask;
        assert(DICT_OK == dict_add(ptask->tasks, name, task));
        ptask->num++;
    } else {
        task = dict_get_entry_val(entry);
    }
    addSeriesTask(task, child);
    /* 若并行任务已在运行中，新增的组需要立即启动 */
    if (entry == NULL && ptask->task.status == DOING_TASK_STATUS) {
        seriesTaskRun(ptask->task.ctx, task);
    }
    return 1;
}
