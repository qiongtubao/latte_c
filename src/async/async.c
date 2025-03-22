#include "async.h"
#include <assert.h>
#include <string.h>
#include "dict/dict_plugins.h"

asyncTask* createAsyncBasicTask(latteThreadJob* job) {
    asyncBasicTask* task = zmalloc(sizeof(asyncBasicTask));
    initTask(task);
    task->task.type = BASIC_TASK_TYPE;
    task->job = job;
    return task;
}

void taskDone(asyncTask* task) {
    task->status = DONE_TASK_STATUS;
    // printf("done: %p\n", task);
    if (task->cb != NULL) {
        task->cb(task);
    } else {
        notifyParent(task);
    }
}

int checkTask(asyncTask* task) {
    if (task->check == NULL) {
        return 1;
    }
    if (!task->check(task)) {
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
    taskDone(task);
}

void basicTaskRun(taskThread* thread, asyncTask* task) {
    asyncBasicTask* basic_task= (asyncBasicTask*)task;
    task->status = DOING_TASK_STATUS;
    // basic_task->job->cb = cb;
    latteThreadJob* job = createThreadJob(basicExec, basicCallback, 1, task);
    submitTask(thread, job);
}

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


void nextParallelTask(asyncTask* task) {
    parallelTask* parallel = task->parent;
    parallel->num--;
    if (parallel->num == 0) {
        taskDone(parallel);
    }
}

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

void notifyParent(asyncTask* task) {
    if (task->parent == NULL) return;
    nextTask(task);
}


int checkSeriesTaskFinished(asyncTask* task) {
    seriesTask* parent = task->parent;
    asyncTask* last = list_node_value(list_last(parent->tasks));
    return last == task;
}




void seriesTaskRun(taskThread* thread, asyncTask* task) {
    // if (!checkTask(task)) return;
    seriesTask* series = (seriesTask*)task;
    if (list_length(series->tasks) == 0) {
        taskDone(series);
        return;
    }
    series->task.status = DOING_TASK_STATUS;
    asyncTask* childTask = list_node_value(list_first(series->tasks));
    // assert(childTask->cb == NULL);
    // childTask->cb = seriesCallback;
    task->ctx = thread;
    asyncRun(thread, childTask);
}




void parallelTaskRun(taskThread* thread,asyncTask* task) {
    parallelTask* ptask = (parallelTask*)task;
    if (dict_size(ptask->tasks) == 0) {
        taskDone(ptask);
        return;
    }
    ptask->task.status = DOING_TASK_STATUS;
    latte_iterator_t *di = dict_get_latte_iterator(ptask->tasks);
    while (latte_iterator_has_next(di)) {
        seriesTask* stask = (seriesTask*)latte_pair_value(latte_iterator_next(di));
        seriesTaskRun(thread, stask);
    }
    latte_iterator_delete(di);
    ptask->task.ctx = thread;
    
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

asyncTask* createSeriesTask() {
    seriesTask* task = zmalloc(sizeof(seriesTask));
    initTask(task);
    task->task.type = SERIES_TASK_TYPE;
    task->tasks = list_new();
    return task;
}

int addSeriesTask(seriesTask* task, asyncTask* child) {
    list_add_node_tail(task->tasks, child);
    child->parent = task;
    return 1;
}


void continueNextTask(asyncTask* task) {
    // switch (task->type)
    // {
    // case BASIC_TASK_TYPE:
    //     /* code */
    //     if (task->parent == NULL) {
    //         return;
    //     } 
    //     switch (task->parent->type)
    //     {
    //     case SERIES_TASK_TYPE:
    //         /* code */
    //         nextSeriesTask(task);
    //         break;
        
    //     default:
    //         break;
    //     }
    //     break;
    
    // default:
    //     break;
    // }
    notifyParent(task);
}


// uint64_t dict_sds_hash(const void *key) {
//     return dict_gen_hash_function((unsigned char*)key, sds_len((char*)key));
// }

// int dict_sds_key_compare(void *privdata, const void *key1,
//         const void *key2)
// {
//     int l1,l2;
//     DICT_NOTUSED(privdata);

//     l1 = sds_len((sds_t)key1);
//     l2 = sds_len((sds_t)key2);
//     if (l1 != l2) return 0;
//     return memcmp(key1, key2, l1) == 0;
// }

// void dict_sds_destructor(void *privdata, void *val)
// {
//     DICT_NOTUSED(privdata);

//     sds_delete(val);
// }

static dict_func_t parallelTaskDictType = {
    dict_sds_hash,                    /* hash function */
    NULL,                           /* key dup */
    NULL,                           /* val dup */
    dict_sds_key_compare,              /* key compare */
    dict_sds_destructor,              /* key destructor */
    NULL,                           /* val destructor */
    NULL                            /* allow to expand */
};
asyncTask* createParallelTask() {
    parallelTask* ptask = zmalloc(sizeof(parallelTask));
    initTask(ptask);
    ptask->task.type = PARALLEL_TASK_TYPE;
    ptask->tasks = dict_new(&parallelTaskDictType);
    ptask->num = 0;
    return ptask;
}

int addParallelTask(parallelTask* ptask, sds_t name, asyncTask* child) {
    if (ptask->task.status == DONE_TASK_STATUS) {
        return 0;
    }
    dict_entry_t* entry = dict_find(ptask->tasks, name);
    asyncTask* task;
    if (entry == NULL) {
        task = createSeriesTask();
        task->parent = ptask;
        assert(DICT_OK == dict_add(ptask->tasks, name, task));
        ptask->num++;
    } else {
        task = dict_get_val(entry);
    }
    addSeriesTask(task, child);
    if (entry == NULL && ptask->task.status == DOING_TASK_STATUS) {
        seriesTaskRun(ptask->task.ctx, task);
    }
    return 1;
}