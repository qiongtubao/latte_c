

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


