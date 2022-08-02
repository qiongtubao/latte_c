
#include "task.h"
#include "../zmalloc/zmalloc.h"

taskThread* createTaskThread(int tnum) {
    taskThread* t = zmalloc(sizeof(taskThread));
    t->num = tnum;
    t->status = THREAD_INIT;
    t->threads = zmalloc(sizeof(latteThread) * tnum);

    for (int j = 0; j < tnum; j++) {
        pthread_mutex_init(&t->threads[j].mutex, NULL);
        pthread_cond_init(&t->threads[j].job_cond, NULL);
        pthread_cond_init(&t->threads[j].step_cond, NULL);
        t->threads[j].jobs = listCreate();
        t->threads[j].pending = 0;
    }
    return t;
}


#define LATTE_THREAD_STACK_SIZE (1024*1024*4)
void initTaskThread(taskThread* thread) {
    pthread_attr_t attr;
    pthread_t t;
    size_t stacksize;

    pthread_attr_init(&attr);
    pthread_attr_getstacksize(&attr,&stacksize);
    if (!stacksize) stacksize = 1;
    while (stacksize < LATTE_THREAD_STACK_SIZE) stacksize *= 2;
    pthread_attr_setstacksize(&attr, stacksize);

    for (int j = 0; j < thread->num; j++) {
        void *arg = (void*)(unsigned long)j;
        if (pthread_create(&t, &attr, taskProcess, arg) != 0) {
            printf("Fatal: Can't initialize Background Jobs.\n");
            exit(1);
        }
        thread->threads[j].thread = t;
    }

}

void *bioProcessBackgroundJobs(void *arg) {
    
}
