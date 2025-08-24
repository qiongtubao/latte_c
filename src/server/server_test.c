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
#include "server.h"
#include "client.h"
#include "cron.h"
#include "log/log.h"

#define PORT 28181
#define BUFFER_SIZE 1024


/**
 * 
 * 
 *  debug log
 *  03 Aug 2025 12:11:31.691 958790 INFO  ae_epoll.c:aeApiCreate:8: [aeApiCreate] ae use epoll
    03 Aug 2025 12:11:31.691 958790 INFO  server.c:start_latte_server:203: start latte server 28181 512 !!!!!
    03 Aug 2025 12:11:31.691 958790 INFO  server.c:start_latte_server:234: start latte server success PID: 958790
    03 Aug 2025 12:11:32.693 958790 INFO  server.c:acceptTcpHandler:332: Accepted 127.0.0.1:54416 7
    03 Aug 2025 12:11:32.693 958790 DEBUG server_test.c:createLatteClient:44: createLatteClient
    03 Aug 2025 12:11:32.693 958790 DEBUG server.c:acceptCommonHandler:302: create client fd:7
    03 Aug 2025 12:11:32.693 958790 DEBUG client.c:link_latte_client:78: link_latte_client 7
    03 Aug 2025 12:11:32.693 958790 DEBUG client.c:read_query_from_client:169: read_query_from_client exec Hello from client 17
    03 Aug 2025 12:11:32.693 958790 DEBUG server_test.c:echoHandler:23: echoHandler Hello from client 17
    03 Aug 2025 12:11:32.693 958790 DEBUG client.c:add_reply_proto:235: add_reply_proto use buffer 17
    03 Aug 2025 12:11:32.693 958790 DEBUG server_test.c:echoHandler:37: echoHandler
    03 Aug 2025 12:11:32.693 958790 DEBUG server.c:handleClientsWithPendingWrites:143: handleClientsWithPendingWrites processed 1
    03 Aug 2025 12:11:32.693 958790 DEBUG client.c:writeToClient:254: writeToClient 7
    03 Aug 2025 12:11:32.693 958790 DEBUG client.c:read_query_from_client:169: read_query_from_client exec quit 4
    03 Aug 2025 12:11:32.693 958790 DEBUG server_test.c:echoHandler:23: echoHandler quit 4
    03 Aug 2025 12:11:32.693 958790 DEBUG server_test.c:echoHandler:28: quit

 */

int echoHandler(struct latte_client_t* lc, int nread) {
    LATTE_LIB_LOG(LOG_DEBUG, "echoHandler %s %d", lc->querybuf, nread);
    // struct client* c = (struct client*)lc; 
    if (strncmp(lc->querybuf, "quit", 4) == 0) {
        lc->qb_pos = 4;
        add_reply_proto(lc, "bye", 3);
        stop_latte_server(lc->server);
        LATTE_LIB_LOG(LOG_DEBUG,"quit");
        return 1;
    }
    // if (connWrite(lc->conn, lc->querybuf, sds_len(lc->querybuf)) == -1) {
    //     printf("write fail");
    // }
    // lc->qb_pos = sds_len(lc->querybuf);
    add_reply_proto(lc, lc->querybuf, sds_len(lc->querybuf));
    lc->qb_pos = sds_len(lc->querybuf);
    LATTE_LIB_LOG(LOG_DEBUG, "echoHandler");
    return 1;
}

void print_time(latte_client_t* client) {
    LATTE_LIB_LOG(LOG_DEBUG, "print_time %ld %ld %ld ", 
        client->exec_time - client->read_time, 
        client->exec_end_time - client->exec_time,
        client->end_time - client->exec_end_time);
}

 latte_client_t *createLatteClient() {
     latte_client_t* client = zmalloc(sizeof(latte_client_t));
    client->exec = echoHandler;
    client->flags = 0;
    LATTE_LIB_LOG(LOG_DEBUG, "createLatteClient");
    client->start = NULL;
    client->end = print_time;
    return client;
}

void freeLatteClient(latte_client_t* client) {
    sds_delete(client->querybuf);
    zfree(client->async_io_request_cache->buf);
    zfree(client->async_io_request_cache);
    list_delete(client->reply);
    zfree(client->conn);
    zfree(client);
}

void _freeLatteClient(void* p) {
    latte_client_t* client = (latte_client_t*)p;
    freeLatteClient(client);
}

vector_t* test_bind_vector_new() {
    vector_t* v = vector_new();
    char* bind[2] = {"*", "-::*"};
    for(int i = 0; i < 2; i++) {
        value_t* val = value_new();
        value_set_sds(val, sds_new(bind[i]));
        vector_push(v, val);
    }
    return v;
}

void print_cron(void* arg) {
    struct latte_server_t* server = (struct latte_server_t*)arg;
    // log_error("latte_lib","print_cron\n");
}

int is_linux() {
    #ifdef __linux__
        LATTE_LIB_LOG(LOG_DEBUG, "is_linux");
        return 1;
    #endif
    return 0;
}

void *server_thread(void *arg) {
    struct latte_server_t* server = zmalloc(sizeof(struct latte_server_t));
    server->port = PORT;
    server->bind = test_bind_vector_new();
    
    server->maxclients = 100;
    server->el = ae_event_loop_new(1024);
    init_latte_server(server);
    server->createClient = createLatteClient;
    server->freeClient = freeLatteClient;
    server->tcp_backlog = 512;
    cron_t* cron = cron_new(print_cron, 1);
    cron_manager_add_cron(server->cron_manager, cron);
    if (is_linux()) {
        async_io_module_init();
        server->use_async_io = true;
    }
    start_latte_server(server);
    LATTE_LIB_LOG(LOG_DEBUG, "server_thread end");
    //end of server
    while (vector_size(server->bind) > 0) {
        value_t* addr = vector_pop(server->bind);
        value_delete(addr);
    }
    vector_delete(server->bind);
    server->clients->free = _freeLatteClient;
    list_delete(server->clients);
    cron_manager_delete(server->cron_manager);
    raxFree(server->clients_index);
    list_delete(server->clients_pending_write);
    list_delete(server->clients_async_pending_write);
    if (is_linux()) {
        async_io_module_destroy();
    }
    zfree(server);
    LATTE_LIB_LOG(LOG_DEBUG, "server_thread end");
}

int test_server() {
    
    pthread_t server_thread_id;

    // 创建线程启动Server
    if (pthread_create(&server_thread_id, NULL, server_thread, NULL) != 0) {
        perror("pthread_create failed");
        exit(EXIT_FAILURE);
    }

    sleep(1); // 等待Server线程启动

    // 创建Socket Client
    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    char *hello = "Hello from client";
    char *quit = "quit";
    char buffer[BUFFER_SIZE] = {0};

    // 创建socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        log_error("latte_c_server","socket failed");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // 将IPv4地址从字符串转换为二进制形式
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        log_error("latte_c_server","inet_pton failed");
        exit(EXIT_FAILURE);
    }

    // 连接到Server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        log_error("latte_c_server","connect failed");
        exit(EXIT_FAILURE);
    }

    // 发送消息给Server
    send(sock, hello, strlen(hello), 0);
    log_debug("latte_c_server","Hello message sent\n");

    // 读取Server的回复
    valread = read(sock, buffer, BUFFER_SIZE);
    assert(strncmp("Hello from client", buffer, 18) == 0);

    send(sock, quit, strlen(quit), 0);
    log_debug("latte_c_server", "Quit message sent\n");

    valread = read(sock, buffer, BUFFER_SIZE);
    LATTE_LIB_LOG(LOG_INFO, "read from server %s %d\n", buffer, valread);
    assert(strncmp("bye", buffer, 3) == 0);
    // 关闭socket
    close(sock);

    // 等待Server线程结束
    pthread_join(server_thread_id, NULL);

    return 1;
}


int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        log_module_init();
        log_add_stdout("latte_lib", LOG_DEBUG);
        test_cond("test server function", 
            test_server() == 1);
    
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}