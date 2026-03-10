/*
 * async_demo.h - async 模块头文件
 * 
 * Latte C 库组件
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */



#include "sds/sds.h"
#include "dict/dict.h"
#include "task/task.h"

typedef int latteTaskCheck(latteTask* task);
typedef void latteTaskCallback(latteTask* task);
typedef void latteTaskExec(latteTask* task);

typedef struct latteTask {
    int type;
    int status;
    latteTask* parent;
    latteTaskCallback* cb;
} latteTask;

typedef struct seriesTask {
    latteTask task;

} seriesTask;


