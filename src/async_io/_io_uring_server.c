#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <liburing.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/timerfd.h>

#define MAX_CONNECTIONS 10240
#define PORT 8080
#define CLIENT_THREADS 4
#define BUFFER_SIZE 1024
#define QPS_INTERVAL 1

// 全局统计
static _Atomic int qps_counter = 0;
static _Atomic int total_connections = 0;
static _Atomic int client_errors = 0;
static _Atomic int data_mismatch = 0;

enum {
    SERVER_ACCEPT,
    CLIENT_CONNECT,
    SOCKET_READ,
    SOCKET_WRITE,
    TIMER_EVENT,
};

struct conn_info {
    int fd;
    int type;
    char buffer[BUFFER_SIZE];
    int data_len;
    struct sockaddr_in client_addr;
    uint64_t request_id;  // 用于验证数据一致性
};

// 生成随机数据
void generate_random_data(char *buffer, int len) {
    for (int i = 0; i < len; i++) {
        buffer[i] = 'A' + (rand() % 26);
    }
}

// 服务端线程函数（完全异步）
void *server_thread(void *arg) {
    struct io_uring ring;
    int server_fd, timer_fd;
    struct sockaddr_in addr;
    
    // 初始化io_uring
    if (io_uring_queue_init(4096, &ring, 0) < 0) {
        perror("io_uring_init");
        exit(EXIT_FAILURE);
    }

    // 创建服务器socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, SOMAXCONN) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    // 创建定时器
    timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    struct itimerspec its = {
        .it_interval = {.tv_sec = QPS_INTERVAL, .tv_nsec = 0},
        .it_value = {.tv_sec = QPS_INTERVAL, .tv_nsec = 0}
    };
    timerfd_settime(timer_fd, 0, &its, NULL);
    
    // 添加ACCEPT和TIMER到io_uring
    struct io_uring_sqe *sqe;
    
    // 1. 添加ACCEPT请求
    sqe = io_uring_get_sqe(&ring);
    struct conn_info *accept_ci = calloc(1, sizeof(struct conn_info));
    accept_ci->fd = server_fd;
    accept_ci->type = SERVER_ACCEPT;
    io_uring_prep_accept(sqe, server_fd, NULL, NULL, 0);
    io_uring_sqe_set_data(sqe, accept_ci);
    
    // 2. 添加TIMER请求
    sqe = io_uring_get_sqe(&ring);
    struct conn_info *timer_ci = calloc(1, sizeof(struct conn_info));
    timer_ci->fd = timer_fd;
    timer_ci->type = TIMER_EVENT;
    io_uring_prep_read(sqe, timer_fd, timer_ci->buffer, BUFFER_SIZE, 0);
    io_uring_sqe_set_data(sqe, timer_ci);
    
    io_uring_submit(&ring);
    
    printf("[Server] Async server started on port %d\n", PORT);
    
    while (1) {
        struct io_uring_cqe *cqe;
        int ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) {
            perror("io_uring_wait_cqe");
            break;
        }
        
        struct conn_info *ci = (struct conn_info *)cqe->user_data;
        if (cqe->res < 0) {
            // 错误处理
            if (ci->type != SERVER_ACCEPT && ci->type != TIMER_EVENT) {
                close(ci->fd);
                total_connections--;
            }
            free(ci);
            io_uring_cqe_seen(&ring, cqe);
            continue;
        }
        
        switch (ci->type) {
            case SERVER_ACCEPT: {
                int client_fd = cqe->res;
                fcntl(client_fd, F_SETFL, O_NONBLOCK);
                
                // 为新连接添加READ请求
                struct conn_info *read_ci = calloc(1, sizeof(struct conn_info));
                read_ci->fd = client_fd;
                read_ci->type = SOCKET_READ;
                
                sqe = io_uring_get_sqe(&ring);
                io_uring_prep_read(sqe, client_fd, read_ci->buffer, BUFFER_SIZE, 0);
                io_uring_sqe_set_data(sqe, read_ci);
                
                // 重新提交ACCEPT请求
                sqe = io_uring_get_sqe(&ring);
                io_uring_prep_accept(sqe, server_fd, NULL, NULL, 0);
                ci->type = SERVER_ACCEPT;
                io_uring_sqe_set_data(sqe, ci);
                
                total_connections++;
                break;
            }
            case SOCKET_READ: {
                int bytes_read = cqe->res;
                if (bytes_read <= 0) {
                    // 连接关闭
                    close(ci->fd);
                    free(ci);
                    total_connections--;
                    break;
                }
                
                // 准备写回相同数据
                ci->data_len = bytes_read;
                ci->type = SOCKET_WRITE;
                
                sqe = io_uring_get_sqe(&ring);
                io_uring_prep_write(sqe, ci->fd, ci->buffer, bytes_read, 0);
                io_uring_sqe_set_data(sqe, ci);
                
                qps_counter++;
                break;
            }
            case SOCKET_WRITE: {
                // 写操作完成后，重新添加READ请求
                ci->type = SOCKET_READ;
                
                sqe = io_uring_get_sqe(&ring);
                io_uring_prep_read(sqe, ci->fd, ci->buffer, BUFFER_SIZE, 0);
                io_uring_sqe_set_data(sqe, ci);
                break;
            }
            case TIMER_EVENT: {
                uint64_t expirations;
                read(timer_fd, &expirations, sizeof(expirations));
                
                printf("[Server] QPS: %d/s | Connections: %d | Errors: %d | Mismatches: %d\n", 
                       qps_counter / QPS_INTERVAL, 
                       total_connections,
                       client_errors,
                       data_mismatch);
                qps_counter = 0;
                
                // 重新提交TIMER请求
                sqe = io_uring_get_sqe(&ring);
                io_uring_prep_read(sqe, timer_fd, ci->buffer, BUFFER_SIZE, 0);
                ci->type = TIMER_EVENT;
                io_uring_sqe_set_data(sqe, ci);
                break;
            }
        }
        
        io_uring_submit(&ring);
        io_uring_cqe_seen(&ring, cqe);
    }
    
    close(server_fd);
    close(timer_fd);
    io_uring_queue_exit(&ring);
    return NULL;
}

// 异步客户端线程函数
void *client_thread(void *arg) {
    struct io_uring ring;
    if (io_uring_queue_init(256, &ring, 0) < 0) {
        perror("client: io_uring_init");
        return NULL;
    }
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
    
    // 创建socket
    int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sock < 0) {
        perror("client socket");
        return NULL;
    }
    
    // 初始化随机种子
    srand(time(NULL) ^ pthread_self());
    
    // 提交连接请求
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    struct conn_info *conn_ci = calloc(1, sizeof(struct conn_info));
    conn_ci->fd = sock;
    conn_ci->type = CLIENT_CONNECT;
    io_uring_prep_connect(sqe, sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    io_uring_sqe_set_data(sqe, conn_ci);
    io_uring_submit(&ring);
    
    // 事件循环
    int connected = 0;
    int requests = 10000; // 每个客户端发送请求数
    char expected_data[BUFFER_SIZE]; // 存储发送的数据用于验证
    
    while (requests > 0) {
        struct io_uring_cqe *cqe;
        int ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) {
            perror("io_uring_wait_cqe");
            client_errors++;
            break;
        }
        
        struct conn_info *ci = (struct conn_info *)cqe->user_data;
        if (cqe->res < 0) {
            // 错误处理
            close(ci->fd);
            free(ci);
            io_uring_cqe_seen(&ring, cqe);
            client_errors++;
            break;
        }
        
        switch (ci->type) {
            case CLIENT_CONNECT: {
                connected = 1;
                // 连接成功后生成随机数据
                int data_len = 64 + (rand() % (BUFFER_SIZE - 64)); // 64-1023字节
                generate_random_data(ci->buffer, data_len);
                ci->data_len = data_len;
                memcpy(expected_data, ci->buffer, data_len); // 保存用于验证
                
                // 提交写请求
                ci->type = SOCKET_WRITE;
                
                sqe = io_uring_get_sqe(&ring);
                io_uring_prep_write(sqe, ci->fd, ci->buffer, data_len, 0);
                io_uring_sqe_set_data(sqe, ci);
                break;
            }
            case SOCKET_WRITE: {
                // 写完成后提交读请求
                ci->type = SOCKET_READ;
                
                sqe = io_uring_get_sqe(&ring);
                io_uring_prep_read(sqe, ci->fd, ci->buffer, ci->data_len, 0);
                io_uring_sqe_set_data(sqe, ci);
                break;
            }
            case SOCKET_READ: {
                int bytes_read = cqe->res;
                
                // 验证数据一致性
                if (bytes_read != ci->data_len || 
                    memcmp(ci->buffer, expected_data, ci->data_len) != 0) {
                    data_mismatch++;
                }
                
                // 准备下一个请求
                requests--;
                if (requests > 0) {
                    // 生成新的随机数据
                    int data_len = 64 + (rand() % (BUFFER_SIZE - 64));
                    generate_random_data(ci->buffer, data_len);
                    ci->data_len = data_len;
                    memcpy(expected_data, ci->buffer, data_len);
                    
                    // 提交写请求
                    ci->type = SOCKET_WRITE;
                    
                    sqe = io_uring_get_sqe(&ring);
                    io_uring_prep_write(sqe, ci->fd, ci->buffer, data_len, 0);
                    io_uring_sqe_set_data(sqe, ci);
                } else {
                    // 完成所有请求，关闭连接
                    close(ci->fd);
                    free(ci);
                }
                break;
            }
        }
        
        io_uring_submit(&ring);
        io_uring_cqe_seen(&ring, cqe);
    }
    
    close(sock);
    io_uring_queue_exit(&ring);
    return NULL;
}

int main() {
    pthread_t server_tid, client_tids[CLIENT_THREADS];
    
    printf("Starting echo server with %d client threads...\n", CLIENT_THREADS);
    
    // 启动服务端线程
    if (pthread_create(&server_tid, NULL, server_thread, NULL) != 0) {
        perror("pthread_create server");
        exit(EXIT_FAILURE);
    }
    
    sleep(1); // 确保服务器先启动
    
    // 启动客户端线程
    for (int i = 0; i < CLIENT_THREADS; i++) {
        if (pthread_create(&client_tids[i], NULL, client_thread, NULL) != 0) {
            perror("pthread_create client");
            continue;
        }
    }
    
    // 等待客户端完成
    for (int i = 0; i < CLIENT_THREADS; i++) {
        pthread_join(client_tids[i], NULL);
    }
    
    // 等待服务器完成
    sleep(2); // 给服务器时间完成最后的统计
    pthread_cancel(server_tid); // 安全终止服务器线程
    
    printf("\nTest completed. Total errors: %d, Data mismatches: %d\n", 
           client_errors, data_mismatch);
    
    return 0;
}
