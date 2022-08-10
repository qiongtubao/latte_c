#include "async.h"
#include <assert.h>


asyncTask* createAsyncBasicTask(int autoNext, latteThreadJob* job) {
    asyncBasicTask* task = zmalloc(sizeof(asyncBasicTask));
    initTask(task);
    task->task.type = BASIC_TASK_TYPE;
    task->task.autoNext = autoNext;
    task->job = job;
    return task;
}

int checkTask(asyncTask* task) {
    if (task->check == NULL) {
        return 1;
    }
    if (!task->check(task)) {
        task->status = DONE_TASK_STATUS;
        if (task->cb!= NULL) task->cb(task);
        return 0;
    }
    return 1;
}

void basicExec(latteThreadJob* job) {
    asyncBasicTask* task = (asyncBasicTask*)job->args[0];
    if (task->job->exec != NULL) {
        task->job->exec(task->job);
    }
}

void basicCallback(latteThreadJob* job) {
    asyncBasicTask* task = (asyncBasicTask*)job->args[0];
    if (task->job->cb != NULL) {
        task->job->cb(task->job);
    }
    task->task.status = DONE_TASK_STATUS;
    if (task->task.cb != NULL) {
        task->task.cb(task);
    }
    // if (task->task.parent)
}

void basicTaskRun(taskThread* thread, asyncTask* task) {
    if (!checkTask(task)) return;
   
    asyncBasicTask* basic_task= (asyncBasicTask*)task;

    // basic_task->job->cb = cb;
    latteThreadJob* job = createThreadJob(basicExec, basicCallback, 1, task);
    submitTask(thread, job);
    task->status = DOING_TASK_STATUS;
}

int checkAutoNext(asyncTask* task) {
    return task->autoNext;
}

void seriesCallback(asyncTask* task) {
    task->status = DONE_TASK_STATUS;
    assert(task->parent != NULL);
    if (checkSeriesTaskFinished(task)) {
        task->parent->status = DONE_TASK_STATUS;
        if(task->parent->cb != NULL) {
            task->parent->cb(task->parent);
        }
        notifyParent(task);
    } else {
        if (checkAutoNext(task)) {
            nextSeriesTask(task);
        } 
    }
}

void notifyParent(asyncTask* task) {
    if (task->parent == NULL) return;
}

void nextSeriesTask(asyncTask* task) {
    seriesTask* parent =  task->parent;
    listNode* node = listSearchKey(parent->tasks, task);
    asyncTask* nextChild = listNodeValue(node->next);
    nextChild->cb = seriesCallback;
    asyncRun(parent->task.ctx, nextChild);
}

int checkSeriesTaskFinished(asyncTask* task) {
    seriesTask* parent = task->parent;
    asyncTask* last = listNodeValue(listLast(parent->tasks));
    return last == task;
}




void seriesTaskRun(taskThread* thread, asyncTask* task) {
    if (!checkTask(task)) return;
    seriesTask* series = (seriesTask*)task;
    if (listLength(series->tasks) == 0) {
        task->status = DONE_TASK_STATUS;
        notifyParent(task);
        return;
    }
    asyncTask* childTask = listNodeValue(listFirst(series->tasks));
    assert(childTask->cb == NULL);
    childTask->cb = seriesCallback;
    task->ctx = thread;
    asyncRun(thread, childTask);
    task->status = DOING_TASK_STATUS;
}

void parallelCallback(asyncTask* task) {
    assert(task->parent != NULL);
    assert(task->status == DONE_TASK_STATUS);
    parallelTask* parallel = task->parent;
    parallel->num--;
    if (parallel->num == 0) {
        parallel->task.status = DONE_TASK_STATUS;
    }
}

void parallelTaskRun(taskThread* thread,asyncTask* task) {
    parallelTask* ptask = (parallelTask*)task;
    dictIterator *di = dictGetIterator(ptask->tasks);
    dictEntry *de;
    while ((de = dictNext(di)) != NULL) {
        seriesTask* stask = dictGetVal(de);
        stask->task.cb = parallelCallback;
        seriesTaskRun(thread, stask);
    }
    dictReleaseIterator(di);
    ptask->task.ctx = thread;
    ptask->task.status = DOING_TASK_STATUS;
}

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

void asyncWait(asyncTask* task) {
    while(1) {
        if (task->status == DONE_TASK_STATUS) {
            return;
        }
        sleep(100);
    }
    return; 
}


//get task result

void* getBasicTaskResult(asyncBasicTask* task) {
    return task->job->result;
}

void initTask(asyncTask* task) {
    task->status = WAIT_TASK_STATUS;
    task->parent = NULL;
    task->check = NULL;
    task->ctx = NULL;
    task->cb = NULL;
}

asyncTask* createSeriesTask(int autoNext) {
    seriesTask* task = zmalloc(sizeof(seriesTask));
    initTask(task);
    task->task.type = SERIES_TASK_TYPE;
    task->task.autoNext = autoNext;
    task->tasks = listCreate();
    return task;
}

int addSeriesTask(seriesTask* task, asyncTask* child) {
    listAddNodeTail(task->tasks, child);
    child->parent = task;
}


void continueNextTask(asyncTask* task) {
    switch (task->type)
    {
    case BASIC_TASK_TYPE:
        /* code */
        if (task->parent == NULL) {
            return;
        } 
        switch (task->parent->type)
        {
        case SERIES_TASK_TYPE:
            /* code */
            nextSeriesTask(task);
            break;
        
        default:
            break;
        }
        break;
    
    default:
        break;
    }
}


uint64_t dictSdsHash(const void *key) {
    return dictGenHashFunction((unsigned char*)key, sdslen((char*)key));
}

int dictSdsKeyCompare(void *privdata, const void *key1,
        const void *key2)
{
    int l1,l2;
    DICT_NOTUSED(privdata);

    l1 = sdslen((sds)key1);
    l2 = sdslen((sds)key2);
    if (l1 != l2) return 0;
    return memcmp(key1, key2, l1) == 0;
}

void dictSdsDestructor(void *privdata, void *val)
{
    DICT_NOTUSED(privdata);

    sdsfree(val);
}

static dictType parallelTaskDictType = {
    dictSdsHash,                    /* hash function */
    NULL,                           /* key dup */
    NULL,                           /* val dup */
    dictSdsKeyCompare,              /* key compare */
    dictSdsDestructor,              /* key destructor */
    NULL,                           /* val destructor */
    NULL                            /* allow to expand */
};
asyncTask* createParallelTask(int autoNext) {
    parallelTask* ptask = zmalloc(sizeof(parallelTask));
    initTask(ptask);
    ptask->task.type = PARALLEL_TASK_TYPE;
    ptask->task.autoNext = autoNext;
    ptask->tasks = dictCreate(&parallelTaskDictType);
    ptask->num = 0;
    return ptask;
}

int addParallelTask(parallelTask* ptask, sds name, asyncTask* child) {
    if (ptask->task.status == DONE_TASK_STATUS) {
        return 0;
    }
    dictEntry* entry = dictFind(ptask->tasks, name);
    asyncTask* task;
    if (entry == NULL) {
        task = createSeriesTask(AUTONEXT);
        task->parent = ptask;
        assert(DICT_OK == dictAdd(ptask->tasks, name, task));
        ptask->num++;
    } else {
        task = dictGetVal(entry);
    }
    addSeriesTask(task, child);
    if (entry == NULL && ptask->task.status == DOING_TASK_STATUS) {
        seriesTaskRun(ptask->task.ctx, task);
    }
    return 1;
}