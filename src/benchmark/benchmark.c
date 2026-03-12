/**
 * @file benchmark.c
 * @brief 基准测试框架实现
 *        基于 ae 事件循环和多线程的客户端基准测试工具，
 *        支持多线程并发测试、异步读写和吞吐量统计。
 */
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






/**
 * @brief 释放单个基准测试线程及其事件循环
 * @param thread 要释放的线程对象指针
 */
static void freeBenchmarkThread(benchmarkThread *thread) {
    if (thread->el) aeDeleteEventLoop(thread->el);
    zfree(thread);
}
/**
 * @brief 释放所有基准测试线程
 */
static void freeBenchmarkThreads() {
    int i = 0;
    for (; i < config.num_threads; i++) {
        benchmarkThread *thread = config.threads[i];
        if (thread) freeBenchmarkThread(thread);
    }
    zfree(config.threads);
    config.threads = NULL;
}

/**
 * @brief 定时事件回调：显示当前吞吐量统计信息
 * @param eventLoop 事件循环指针
 * @param id        定时器 id
 * @param clientData 用户数据（未使用）
 * @return 下次触发的毫秒间隔
 */
int showThroughput(struct aeEventLoop *eventLoop, long long id, void *clientData) {
    /*TODO 显示吞吐量*/
}

/**
 * @brief 创建一个基准测试线程并初始化其事件循环
 * @param index 线程索引编号
 * @return 新建的 benchmarkThread 指针；内存分配失败返回 NULL
 */
static benchmarkThread *createBenchmarkThread(int index) {
    benchmarkThread *thread = zmalloc(sizeof(*thread));
    if (thread == NULL) return NULL;
    thread->index = index;
    thread->el = aeCreateEventLoop(1024*10);
    aeCreateTimeEvent(thread->el,1,showThroughput,NULL,NULL);
    return thread;
}

/**
 * @brief 初始化所有基准测试线程（若已存在则先释放重建）
 */
static void initBenchmarkThreads() {
    int i;
    if (config.threads) freeBenchmarkThreads();
    config.threads = zmalloc(config.num_threads * sizeof(benchmarkThread*));
    for (i = 0; i < config.num_threads; i++) {
        benchmarkThread *thread = createBenchmarkThread(i);
        config.threads[i] = thread;
    }
}

/**
 * @brief 基准测试线程入口函数，运行 ae 事件循环
 * @param ptr benchmarkThread 指针
 * @return NULL
 */
static void *execBenchmarkThread(void *ptr) {
    benchmarkThread *thread = (benchmarkThread *) ptr;
    aeMain(thread->el);
    return NULL;
}
/**
 * @brief 启动所有基准测试线程并等待它们全部完成
 */
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
/**
 * @brief 创建到服务器的基准测试上下文连接
 * @param ip   服务器 IP 地址
 * @param port 服务器端口号
 * @return 新建的 benchmarkContext 指针
 */
benchmarkContext* createContext(char* ip, int port) {
    benchmarkContext* context = zmalloc(sizeof(benchmarkContext));
    
    return context;
}

#define CLIENT_GET_EVENTLOOP(c) \
    (c->thread_id >= 0 ? config.threads[c->thread_id]->el : config.el)

/**
 * @brief 释放客户端连接及其关联的事件注册
 * @param c 要释放的客户端指针
 */
static void freeClient(client c) {
    aeEventLoop *el = CLIENT_GET_EVENTLOOP(c);
}

/**
 * @brief ae 可读事件回调：处理服务器响应数据
 * @param el       事件循环指针
 * @param fd       可读文件描述符
 * @param privdata 客户端私有数据
 * @param mask     触发的事件掩码
 */
static void readHandler(aeEventLoop *el, int fd, void *privdata, int mask) {

}

/**
 * @brief 向服务器写入数据
 * @param c       基准测试上下文
 * @param buf     要发送的数据缓冲区
 * @param buf_len 数据长度
 * @return 实际写入的字节数
 */
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

/**
 * @brief ae 可写事件回调：向服务器发送命令数据
 *        首次写入时初始化请求内容；循环写直到全部发出，
 *        完成后切换为可读事件等待响应。
 * @param el       事件循环指针
 * @param fd       可写文件描述符
 * @param privdata 客户端私有数据
 * @param mask     触发的事件掩码
 */
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
/**
 * @brief 创建客户端并注册写事件，准备发送基准测试命令
 * @param cmd       要发送的命令字符串
 * @param len       命令长度
 * @param from      参考客户端（可为 NULL）
 * @param thread_id 绑定的线程 id（-1 表示使用主事件循环）
 * @return 新建的 client 指针
 */
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
/**
 * @brief 初始化 benchmark 结构体
 * @param be 要初始化的 benchmark 指针
 * @return 1 成功
 */
int initBenchmark(struct benchmark* be) {
    be->clients = list_new();
    be->threads = NULL;
    be->liveclients = 0;
    be->el = aeCreateEventLoop(1024*10);
    return 1;
}

/**
 * @brief 启动基准测试：创建客户端并运行事件循环或多线程
 * @param be    benchmark 实例
 * @param title 测试标题（用于统计输出）
 * @param cmd   要发送的基准测试命令
 * @param len   命令长度
 * @return 0 成功（TODO: 完善返回值）
 */
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