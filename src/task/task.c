
#include "task.h"
#include "zmalloc/zmalloc.h"
#include <pthread.h>
#include <inttypes.h>
#include <signal.h>
#include <errno.h>
#include <stdarg.h>
#include "anet/anet.h"

latteThreadJob* createThreadJob(task_fn tfn, callback_fn cb, int arg_count, ...) {
    va_list valist;
    /* Allocate memory for the job structure and all required
     * arguments */
    latteThreadJob *job = zmalloc(sizeof(latteThreadJob));
    job->exec = tfn;
    job->cb = cb;
    job->argv = arg_count;
    job->args = zmalloc(sizeof(void*) * arg_count);
    va_start(valist, arg_count);
    for (int i = 0; i < arg_count; i++) {
        job->args[i] = va_arg(valist, void *);
    }
    va_end(valist);
    return job;

}


void notifyCallbackHandler(latteThread* thread) {
     /* Pop the job from the queue. */
    listNode *ln = listFirst(thread->recv_queue);
    while (ln != NULL) {
        struct latteThreadJob *job = ln->value;
        if (job->cb != NULL) {
            job->cb(job);
        }
        pthread_mutex_lock(&thread->mutex);
        listDelNode(thread->recv_queue,ln);
        pthread_mutex_unlock(&thread->mutex);
        //TODO free job
        ln = listFirst(thread->recv_queue);
    }
}

void taskCallbackHandler(aeEventLoop *el, int fd, void* privdata, int  mask) {
    char notify_recv_buf[512];
    int nread = read(fd, notify_recv_buf, sizeof(notify_recv_buf));
    if (nread == 0) {
        printf("[rocks] notify recv fd closed.\n");
    } else if (nread < 0) {
        printf("[rocks] read notify failed: %s\n",
                strerror(errno));
    } else {
        switch (notify_recv_buf[0]) {
            case 'x':
                notifyCallbackHandler(privdata);
                break;
        }
    }

}

int taskThreadInit(int id, latteThread* thread, aeEventLoop* el) {
    pthread_mutex_init(&thread->mutex, NULL);
    pthread_cond_init(&thread->job_cond, NULL);
    pthread_cond_init(&thread->step_cond, NULL);
    thread->send_queue = listCreate();
    thread->recv_queue = listCreate();
    thread->pending = 0;
    thread->tid = id;
    int fds[2];
    if (pipe(fds)) {
        printf("can't create pipe");
        return 0;
    }
    thread->notify_recv_fd = fds[0];
    thread->notify_send_fd = fds[1];
    char anetErr[256];
    if (anetNonBlock(anetErr, thread->notify_recv_fd) != 0) {
        printf(
                "Fatal: set notify_recv_fd non-blocking failed: %s",
                anetErr);
        return 0;
    }

    if (anetNonBlock(anetErr, thread->notify_send_fd) != 0) {
        printf("Fatal: set notify_recv_fd non-blocking failed: %s",
                anetErr);
        return 0;
    }

    if (el != NULL && 
        aeCreateFileEvent(el, thread->notify_recv_fd,
                AE_READABLE, taskCallbackHandler, thread) == AE_ERR) {
        printf("Fatal: create notify recv event failed: %s",
                strerror(errno));
        return 0;
    }
    return 1;
}
taskThread* createTaskThread(int tnum, aeEventLoop* el) {
    taskThread* t = zmalloc(sizeof(taskThread));
    t->num = tnum;
    t->status = THREAD_INIT;
    t->threads = zmalloc(sizeof(latteThread) * tnum);

    for (int j = 0; j < tnum; j++) {
        if (taskThreadInit(j, &t->threads[j], el) == 0) {
            //TODO free
            return NULL;
        }
        
    }
    return t;
}


/* Make the thread killable at any time, so that kill threads functions
 * can work reliably (default cancelability type is PTHREAD_CANCEL_DEFERRED).
 * Needed for pthread_cancel used by the fast memory test used by the crash report. */
void makeThreadKillable(void) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
}

int notifyCallback(latteThread* thread) {
    if (write(thread->notify_send_fd, "x", 1) < 1 && errno != EAGAIN) {
        printf("notifyCallback fail\n");
        return -1;
    } 
    return 1;
}

void *taskProcess(void *arg) {
    struct latteThreadJob *job;
    latteThread* thread = (latteThread*) arg;
    sigset_t sigset;


    latteSetCpuAffinity("");

    makeThreadKillable();

    pthread_mutex_lock(&thread->mutex);
    /* Block SIGALRM so we are sure that only the main thread will
     * receive the watchdog signal. */
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGALRM);
    if (pthread_sigmask(SIG_BLOCK, &sigset, NULL))
            printf("Warning: can't mask SIGALRM in bio.c thread: %s\n", strerror(errno));

    while(1) {
        listNode *ln;
        if (listLength(thread->send_queue) == 0) {
            pthread_cond_wait(&thread->job_cond, &thread->mutex);
            continue;
        }
         /* Pop the job from the queue. */
        ln = listFirst(thread->send_queue);
        job = ln->value;

        pthread_mutex_unlock(&thread->mutex);
        int error = 0;
        job->exec(job);
        pthread_mutex_lock(&thread->mutex);
        listAddNodeTail(thread->recv_queue, job);
        notifyCallback(thread);
        listDelNode(thread->send_queue,ln);
        thread->pending--;
        pthread_cond_broadcast(&thread->step_cond);

    }


}

#define LATTE_THREAD_STACK_SIZE (1024*1024*4)
void startTaskThread(taskThread* thread) {
    pthread_attr_t attr;
    pthread_t t;
    size_t stacksize;

    pthread_attr_init(&attr);
    pthread_attr_getstacksize(&attr,&stacksize);
    if (!stacksize) stacksize = 1;
    while (stacksize < LATTE_THREAD_STACK_SIZE) stacksize *= 2;
    pthread_attr_setstacksize(&attr, stacksize);

    for (int j = 0; j < thread->num; j++) {
        if (pthread_create(&t, &attr, taskProcess, thread->threads + j) != 0) {
            printf("Fatal: Can't initialize Background Jobs.\n");
            exit(1);
        }
        thread->threads[j].thread = t;
    }
}

void stopTaskThread(taskThread* thread) {
    int err, j;
    for (j = 0; j < thread->num; j++) {
        latteThread* t =  thread->threads + j;
        if (t->thread == pthread_self()) continue;
        if (t->thread && pthread_cancel(t->thread) == 0) {
            if ((err = pthread_join(t->thread,NULL)) != 0) {
                printf("Bio thread for job type #%d can not be joined: %s\n",
                        j, strerror(err));
            } else {
                printf("Bio thread for job type #%d terminated\n",j);
            }
        }
    }
}

void latteSetCpuAffinity(const char *cpulist) {
#ifdef USE_SETCPUAFFINITY
    setcpuaffinity(cpulist);
#else
    UNUSED(cpulist);
#endif
}

int trySubmitTask(taskThread* thread, int i, latteThreadJob* t) {
    latteThread* l = thread->threads + i;
    if (thread->maxJobs > 0 && thread->maxJobs <= l->pending) {
        return 0;
    }
    pthread_mutex_lock(&l->mutex);
    listAddNodeTail(l->send_queue, t);
    l->pending++;
    pthread_cond_signal(&l->job_cond);
    pthread_mutex_unlock(&l->mutex);
    return 1;
}


void submitTask(taskThread* thread, latteThreadJob* t) {
    static int i = -1;
    while (1) {  
        i++;
        if (trySubmitTask(thread,  i%(thread->num),  t)) {
            break;
        }
    }
}

