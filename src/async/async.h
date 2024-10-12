


#include "sds/sds.h"
#include "dict/dict.h"
#include "task/task.h"

#define BASIC_TASK_TYPE 0
#define SERIES_TASK_TYPE 1
#define PARALLEL_TASK_TYPE 2


#define WAIT_TASK_STATUS 0
#define DOING_TASK_STATUS 1
#define DONE_TASK_STATUS 2


typedef void asyncCallback(void* task);
typedef int checkAsyncTask(void* task);
typedef struct asyncTask {
    int type;
    int status;
    struct asyncTask* parent;
    checkAsyncTask* check;
    asyncCallback* cb;
    void* ctx;
} asyncTask;

typedef struct asyncBasicTask {
    asyncTask task;
    latteThreadJob* job;
} asyncBasicTask;

typedef struct seriesTask {
    /* data */
    asyncTask task;
    list* tasks;
} seriesTask;

typedef struct parallelTask {
  asyncTask task;
  dict* tasks;
  int num;
} parallelTask;

asyncTask* createAsyncBasicTask(latteThreadJob* job);
void asyncRun(taskThread* thread,asyncTask* task);
void asyncWait(asyncTask* task);
void* getBasicTaskResult(asyncBasicTask* task);


asyncTask* createSeriesTask();
int addSeriesTask(seriesTask* task, asyncTask* child);
void nextSeriesTask(asyncTask* current);
void continueNextTask(asyncTask* task);

asyncTask* createParallelTask();
int addParallelTask(parallelTask* task, sds_t name, asyncTask* child);
 