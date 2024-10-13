
#ifndef __LATTE_BENCHMARK_H
#define __LATTE_BENCHMARK_H
#include <stdio.h>
#include <stdlib.h>
#include "sds/sds.h"
#include "ae/ae.h"
#include "utils/atomic.h"
#include "list/list.h"
typedef int *(*writeConnFunc)(void *context, void* ptr, ssize_t len);
typedef ssize_t *(*readConnFunc)(void *context);

typedef struct benchmarkContext {
    int err; /* Error flags, 0 when there is no error */
    char errstr[128]; /* String representation of error when applicable */
    int fd;
    writeConnFunc write;
    readConnFunc read;
} benchmarkContext;

typedef ssize_t *(*writeContextFunc)(void *client, void* ptr, ssize_t len);
typedef ssize_t *(*readHandlerFunc)(void *client);
typedef ssize_t *(*createWriteContentFunc)(void *client);
typedef struct _client {
    benchmarkContext *context;
    sds_t obuf; /* 发送的缓存 */
    int thread_id;
    int slots_last_update;
    int written;
    long long start;        /* Start time of a request */
    long long latency;      /* Request latency */
    createWriteContentFunc createWriteContent;

    writeContextFunc writeContext;
    readHandlerFunc readHandler;
} *client;

/** Thread */
typedef struct benchmarkThread {
    int index;
    pthread_t thread;
    aeEventLoop *el;
} benchmarkThread;


typedef struct benchmark {
    int num_threads;
    aeEventLoop *el;
    const char *hostip;
    int hostport;
    struct benchmarkThread **threads;
    list_t *clients;
    latteAtomic int liveclients;
    latteAtomic int slots_last_update;/* 插槽最后更新 */
} benchmark;

/* benchmark method */
int initBenchmark(struct benchmark* be);
int startBenchmark(struct benchmark* be, char *title, char *cmd, int len);


#endif