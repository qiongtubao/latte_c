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


#define PORT 8081
#define BUFFER_SIZE 1024



int echoHandler(struct latteClient* lc) {
    log_info("latte_c_server", "echoHandler\n");
    struct client* c = (struct client*)lc; 
    if (strncmp(lc->querybuf, "quit", 4) == 0) {
        lc->qb_pos = 4;
        stopServer(lc->server);
        return 1;
    }
    if (connWrite(lc->conn, lc->querybuf, sds_len(lc->querybuf)) == -1) {
        printf("write fail");
    }
    lc->qb_pos = sds_len(lc->querybuf);
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
void *server_thread(void *arg) {
    struct latteServer* server = zmalloc(sizeof(struct latteServer));
    server->port = PORT;
    server->bind = test_bind_vector_new();
    
    server->maxclients = 100;
    server->el = aeCreateEventLoop(1024);
    initInnerLatteServer(server);
    server->createClient = createLatteClient;
    server->freeClient = freeLatteClient;
    startLatteServer(server);
}

int test_server() {
    log_init();
    log_add_stdout("latte_c", LOG_DEBUG);
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
        test_cond("test server function", 
            test_server() == 1);
    
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}