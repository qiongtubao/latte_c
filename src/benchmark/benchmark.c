#include "benchmark.h"
#include "ae/ae.h"
#include "zmalloc/zmalloc.h"
#include "sds/sds.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils/utils.h"
#include "utils/atomic.h"
#include "list/list.h"
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
static struct beancmarkConfig {
    int num_threads;
    aeEventLoop *el;
    const char *hostip;
    int hostport;
    struct benchmarkThread **threads;
    list_t *clients;
    latteAtomic int liveclients;
    latteAtomic int slots_last_update;/* 插槽最后更新 */
} config;
typedef int *(*writeConnFunc)(void *context, void* ptr, ssize_t len);
typedef ssize_t *(*readConnFunc)(void *context);






static void freeBenchmarkThread(benchmarkThread *thread) {
    if (thread->el) aeDeleteEventLoop(thread->el);
    zfree(thread);
}
static void freeBenchmarkThreads() {
    int i = 0;
    for (; i < config.num_threads; i++) {
        benchmarkThread *thread = config.threads[i];
        if (thread) freeBenchmarkThread(thread);
    }
    zfree(config.threads);
    config.threads = NULL;
}

int showThroughput(struct aeEventLoop *eventLoop, long long id, void *clientData) {
    /*TODO 显示吞吐量*/
}

static benchmarkThread *createBenchmarkThread(int index) {
    benchmarkThread *thread = zmalloc(sizeof(*thread));
    if (thread == NULL) return NULL;
    thread->index = index;
    thread->el = aeCreateEventLoop(1024*10);
    aeCreateTimeEvent(thread->el,1,showThroughput,NULL,NULL);
    return thread;
}

static void initBenchmarkThreads() {
    int i;
    if (config.threads) freeBenchmarkThreads();
    config.threads = zmalloc(config.num_threads * sizeof(benchmarkThread*));
    for (i = 0; i < config.num_threads; i++) {
        benchmarkThread *thread = createBenchmarkThread(i);
        config.threads[i] = thread;
    }
}

static void *execBenchmarkThread(void *ptr) {
    benchmarkThread *thread = (benchmarkThread *) ptr;
    aeMain(thread->el);
    return NULL;
}
static void startBenchmarkThreads() {
    int i;
    for (i = 0; i < config.num_threads; i++) {
        benchmarkThread *t = config.threads[i];
        if (pthread_create(&(t->thread), NULL, execBenchmarkThread, t)){
            fprintf(stderr, "FATAL: Failed to start thread %d.\n", i);
            exit(1);
        }
    }
    for (i = 0; i < config.num_threads; i++)
        pthread_join(config.threads[i]->thread, NULL);
}
benchmarkContext* createContext(char* ip, int port) {
    benchmarkContext* context = zmalloc(sizeof(benchmarkContext));
    
    return context;
}

#define CLIENT_GET_EVENTLOOP(c) \
    (c->thread_id >= 0 ? config.threads[c->thread_id]->el : config.el)

static void freeClient(client c) {
    aeEventLoop *el = CLIENT_GET_EVENTLOOP(c);
}

static void readHandler(aeEventLoop *el, int fd, void *privdata, int mask) {

}

ssize_t writeConn(benchmarkContext *c, const char *buf, size_t buf_len){
    return 0;
}
// static long long ustime(void) {
//     struct timeval tv;
//     long long ust;

//     gettimeofday(&tv, NULL);
//     ust = ((long long)tv.tv_sec)*1000000;
//     ust += tv.tv_usec;
//     return ust;
// }

static void writeHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
    client c = privdata;
    /* Initialize request when nothing was written. */
    if (c->written == 0) {
        c->createWriteContent(c);
        latte_atomic_get(config.slots_last_update, c->slots_last_update);
        c->start = ustime();
        c->latency = -1;
    } 
    const ssize_t buflen = sds_len(c->obuf);
    const ssize_t writeLen = buflen-c->written;
    if (writeLen > 0) {
        void *ptr = c->obuf+c->written;
        while(1) {
            /* Optimistically try to write before checking if the file descriptor
             * is actually writable. At worst we get EAGAIN. */
            const ssize_t nwritten = c->writeContext(c->context,ptr,writeLen);
            if (nwritten != writeLen) {
                if (errno != EPIPE) {
                    fprintf(stderr, "Error writing to the server: %s\n", strerror(errno));
                }
                freeClient(c);
                return;
            } else if (nwritten > 0) {
                c->written += nwritten;
                return;
            } else {
                aeDeleteFileEvent(el,c->context->fd,AE_WRITABLE);
                aeCreateFileEvent(el,c->context->fd,AE_READABLE,readHandler,c);
                return;
            }
        }
    }
}
static client createClient(char *cmd, size_t len, client from, int thread_id) {
    client c = zmalloc(sizeof(struct _client));
    c->obuf = sds_empty();
    // if (config.hostsocket == NULL) {
        c->context = createContext(config.hostip, config.hostport);
    // }
    
    if (c->context->err) {
        fprintf(stderr,"Could not connect to server at ");
        // if (config.hostsocket == NULL)
            fprintf(stderr,"%s:%d: %s\n",config.hostip,config.hostport,c->context->errstr);
        // else
            // fprintf(stderr,"%s: %s\n",config.hostsocket,c->context->errstr);
        exit(1);
    }
    c->thread_id = thread_id;
    c->obuf = sds_empty();
    c->written = 0;
    aeEventLoop *el = NULL;
    if (thread_id < 0) el = config.el;
    else {
        benchmarkThread *thread = config.threads[thread_id];
        el = thread->el;
    }
    aeCreateFileEvent(el,c->context->fd,AE_WRITABLE,writeHandler,c);
    list_add_node_tail(config.clients,c);
    latte_atomic_incr(config.liveclients, 1);
    latte_atomic_get(config.slots_last_update, c->slots_last_update);
}



/* benchmark method */
int initBenchmark(struct benchmark* be) {
    be->clients = list_new();
    be->threads = NULL;
    be->liveclients = 0;
    be->el = aeCreateEventLoop(1024*10);
    return 1;
}

int startBenchmark(struct benchmark* be, char *title, char *cmd, int len) {
    client c;

    if (config.num_threads) initBenchmarkThreads();
    
    int thread_id = config.num_threads > 0 ? 0 : -1;
    //注册写入事件
    c = createClient(cmd,len,NULL,thread_id);

    if (!config.num_threads) {
        aeMain(config.el);
    } else {
        startBenchmarkThreads();
    }
}

// int main(int argc, const char **argv) {
//     client c;

//     initConfig(argc, argv);
//     aeCreateTimeEvent(config.el,1,showThroughput,NULL,NULL);
//     benchmark("PING_INLINE","PING\r\n",6);

// }