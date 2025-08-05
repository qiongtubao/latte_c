#include "../test/testhelp.h"
#include "../test/testassert.h"
#include "async_io.h"
#include "debug/latte_debug.h"
#include "utils/utils.h"
#include "log/log.h"
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#define MAX_EVENTS 1024
#define PORT 28080
#define CLIENT_THREADS 4
#define BUFFER_SIZE 16


typedef struct {
    bool is_stop;
    bool read_async_io;
    bool write_async_io;
} thread_info;

struct conn_info {
    int fd;
    char buffer[BUFFER_SIZE];
};

static long long qps_counter = 0;

void server_send_ok( async_io_request_t* request) {
    qps_counter++;
    async_io_request_delete(request);
}
// 服务端线程函数
void *server_thread(void *arg) {
    thread_info* info = (thread_info*)arg;
    int server_fd, epoll_fd, timer_fd;
    struct epoll_event ev, events[MAX_EVENTS];
    struct sockaddr_in addr;

    async_io_module_thread_init();
    // 创建服务器socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, SOMAXCONN) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    // 初始化epoll
    if ((epoll_fd = epoll_create1(0)) == -1) {
        perror("epoll_create");
        exit(EXIT_FAILURE);
    }
    
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        perror("epoll_ctl");
        exit(EXIT_FAILURE);
    }
    
    // 创建定时器
    if ((timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK)) == -1) {
        perror("timerfd_create");
        exit(EXIT_FAILURE);
    }
    
    struct itimerspec its = {
        .it_interval = {.tv_sec = 1, .tv_nsec = 0},
        .it_value = {.tv_sec = 1, .tv_nsec = 0}
    };
    timerfd_settime(timer_fd, 0, &its, NULL);
    
    ev.events = EPOLLIN;
    ev.data.fd = timer_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timer_fd, &ev) == -1) {
        perror("epoll_ctl timer");
        exit(EXIT_FAILURE);
    }
    

    printf("Server started on port %d\n", PORT);

    while (!info->is_stop) {
        // 处理io_uring完成事件
        // struct io_uring_cqe *cqe;
        // unsigned head;
        // unsigned count = 0;
        
        // io_uring_for_each_cqe(&ring, head, cqe) {
        //     if (cqe->res < 0) {
        //         fprintf(stderr, "Async write failed: %s\n", strerror(-cqe->res));
        //     }
        //     struct conn_info *ci = io_uring_cqe_get_data(cqe);
        //     if (ci) {
        //         // close(ci->fd);
        //         free(ci);
        //     }
        //     count++;
        //     qps_counter++;
        // }
        // io_uring_cq_advance(&ring, count);
        async_io_each_finished();
        // 等待epoll事件
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            continue;
        }
        
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == timer_fd) {
                uint64_t expirations;
                read(timer_fd, &expirations, sizeof(expirations));
                
                // pthread_mutex_lock(&counter_mutex);
                printf("QPS: %d\n", qps_counter);
                qps_counter = 0;
                // pthread_mutex_unlock(&counter_mutex);
            } 
            else if (events[i].data.fd == server_fd) {
                // 接受新连接
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
                if (client_fd == -1) {
                    perror("accept");
                    continue;
                }
                
                fcntl(client_fd, F_SETFL, O_NONBLOCK);
                
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                    perror("epoll_ctl client");
                    close(client_fd);
                }
            } 
            else {
                // 处理客户端数据
                struct conn_info *ci = zmalloc(sizeof(struct conn_info));
                if (!ci) {
                    perror("malloc");
                    close(events[i].data.fd);
                    continue;
                }
                
                ci->fd = events[i].data.fd;
                ssize_t len = 0;
                if (info->read_async_io) {
                    //TODO
                } else {
                    len = read(ci->fd, ci->buffer, BUFFER_SIZE);
                    if (len <= 0) {
                        close(ci->fd);
                        free(ci);
                        continue;
                    }
                }
                if (info->write_async_io) {
                    // 提交异步写请求
                    // struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
                    // if (!sqe) {
                    //     fprintf(stderr, "No free SQEs\n");
                    //     close(ci->fd);
                    //     free(ci);
                    //     continue;
                    // }
                    
                    // io_uring_prep_send(sqe, ci->fd, ci->buffer, len, 0);
                    // io_uring_sqe_set_data(sqe, ci);
                    // io_uring_submit(&ring);
                    async_io_net_write(net_write_request_new(ci->fd, ci->buffer, len, NULL, server_send_ok));
                } else {
                    if (write(ci->fd, ci->buffer, len) < 0) {
                        printf("client write error\n");
                        perror("client write");
                        break;
                    }
                }
                zfree(ci);
                // pthread_mutex_lock(&counter_mutex);
                
                // pthread_mutex_unlock(&counter_mutex);
            }
        }
    }
    close(server_fd);
    close(epoll_fd);
    close(timer_fd);
    async_io_module_thread_destroy();
    return NULL;
}

// 客户端线程函数
void *client_thread(void *arg) {
    thread_info* info = (thread_info*)arg;
    async_io_module_thread_init();
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("client socket");
        return NULL;
    }
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sock);
        return NULL;
    }
    
    char buffer[BUFFER_SIZE];
    memset(buffer, 'A', BUFFER_SIZE);
    while (!info->is_stop) {
        // 发送数据
        if (info->write_async_io) {
            //TODO
            // async_io_net_write(async_io, buffer, BUFFER_SIZE);
        } else {
            if (write(sock, buffer, BUFFER_SIZE) < 0) {
                printf("client write error\n");
                perror("client write");
                break;
            }
        }
        if (info->read_async_io) {
            //TODO
            // async_io_net_read(async_io, buffer, BUFFER_SIZE);
        } else {
            // 接收响应
            if (read(sock, buffer, BUFFER_SIZE) < 0) {
                perror("client read");
                printf("client read error\n");
                break;
            }
        }
        // 添加延迟避免过度消耗CPU
        // usleep(10);
    }
    
    close(sock);
    async_io_module_thread_destroy();
    return NULL;
}

int test_server(bool server_read_async_io, bool server_write_async_io, bool client_read_async_io, bool client_write_async_io) {
    pthread_t server_tid, client_tids[CLIENT_THREADS];
    thread_info server_info = {
        .is_stop = false,
        .read_async_io = server_read_async_io,
        .write_async_io = server_write_async_io,
    };
    thread_info client_info = {
        .is_stop = false,
        .read_async_io = client_read_async_io,
        .write_async_io = client_write_async_io,
    };
    // 启动服务端线程
    if (pthread_create(&server_tid, NULL, server_thread, &server_info) != 0) {
        perror("pthread_create server");
        exit(EXIT_FAILURE);
    }
    
    sleep(1); // 确保服务器先启动
    // 启动客户端线程
    for (int i = 0; i < CLIENT_THREADS; i++) {
        if (pthread_create(&client_tids[i], NULL, client_thread, &client_info) != 0) {
            perror("pthread_create client");
            continue;
        }
    }
    sleep(10);
    
    client_info.is_stop = true;
    
    for (int i = 0; i < CLIENT_THREADS; i++) {
        pthread_join(client_tids[i], NULL);
    }
    server_info.is_stop = true;
    pthread_join(server_tid, NULL);
    return 1;
}



int test_api(void) {
    log_module_init();
    async_io_module_init();
    assert(log_add_stdout(LATTE_LIB, LOG_DEBUG) == 1);
    {
        
        test_cond("async io net write", 
            test_server(false, true, false, false) == 1);
        
    } test_report()
    async_io_module_destroy();
    return 1;
}

int main() {
    test_api();
    return 0;
}