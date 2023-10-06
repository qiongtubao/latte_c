#include "test/testhelp.h"
#include "test/testassert.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "server/server.h"
#include "benchmark.h"

#define PORT 8081
#define BUFFER_SIZE 1024
int echoHandler(struct latteClient* lc) {
    printf("echoHandler\n");
    struct client* c = (struct client*)lc; 
    if (strncmp(lc->querybuf, "quit", 4) == 0) {
        lc->qb_pos = 4;
        stopServer(lc->server);
        return 1;
    }
    if (connWrite(lc->conn, lc->querybuf, sdslen(lc->querybuf)) == -1) {
        printf("write fail");
    }
    lc->qb_pos = sdslen(lc->querybuf);
    return 1;
}
struct latteClient *createLatteClient() {
    struct latteClient* client = zmalloc(sizeof(struct latteClient));
    client->exec = echoHandler;
    return client;
}

void freeLatteClient(struct latteClient* client) {
    zfree(client);
}
void *server_thread(void *arg) {
    struct latteServer* server = zmalloc(sizeof(struct latteServer));
    server->port = PORT;
    server->bind = NULL;
    server->maxclients = 100;
    server->el = aeCreateEventLoop(1024);
    initInnerLatteServer(server);
    server->createClient = createLatteClient;
    server->freeClient = freeLatteClient;
    startLatteServer(server);
}
int test_benchmark() {
    pthread_t server_thread_id;

    // 创建线程启动Server
    if (pthread_create(&server_thread_id, NULL, server_thread, NULL) != 0) {
        perror("pthread_create failed");
        exit(EXIT_FAILURE);
    }

    sleep(1); // 等待Server线程启动

    //启动benchmark
    struct benchmark* be = zmalloc(sizeof(benchmark));
    initBenchmark(be);
    be->hostip = "127.0.0.1";
    be->hostport = PORT;

    startBenchmark(be, "PING_INLINE","PING\r\n", 6);
    // aeCreateTimeEvent(el,1,showThroughput,NULL,NULL);
    // benchmark();
    return 1;
}


int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("test server function", 
            test_benchmark() == 1);
    
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}