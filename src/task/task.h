
#include "../utils/atomic.h"
#include "../list/list.h"


typedef void task_fn(void *args[]);
typedef int callback_fn(int error, void* result);
typedef struct tObj {
    task_fn fn;
    int argv;
    void** args;
    callback_fn cb;
} tObj;


#define THREAD_INIT 0
#define THREAD_RUNNING 1

typedef struct latteThread {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t job_cond;
    pthread_cond_t step_cond;
    list* jobs;
    int pending;
} latteThread;
typedef struct taskThread {
    int num;
    latteAtomic int status;
    latteThread* threads;
} taskThread;

taskThread* createTaskThread(int tnum);
void initTaskThread(taskThread* thread);
void CreateTaskJob(task_fn tfn, callback_fn cb, int arg_count, ...);

