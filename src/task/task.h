
#include "../utils/atomic.h"
#include "list/list.h"


typedef int task_fn(void* job);
typedef void callback_fn(void* job);

typedef struct latteThreadJob {
    int argv;
    void** args;
    void* result;
    int error;
    int id;
    task_fn *exec;
    callback_fn *cb;
} latteThreadJob;


#define THREAD_INIT 0
#define THREAD_RUNNING 1

typedef struct latteThread {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t job_cond;
    pthread_cond_t step_cond;
    list* jobs;
    int pending;
    int tid;
} latteThread;
typedef struct taskThread {
    int num;
    latteAtomic int status;
    latteThread* threads;
    //config
    int maxJobs;
} taskThread;

taskThread* createTaskThread(int tnum);
void startTaskThread(taskThread* thread);
void stopTaskThread(taskThread* thread);
latteThreadJob* createThreadJob(task_fn tfn, callback_fn cb, int arg_count, ...);

