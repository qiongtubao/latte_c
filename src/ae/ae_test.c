#include "../test/testhelp.h"
#include "../test/testassert.h"
#include "debug/latte_debug.h"
#include "utils/utils.h"
#include "log/log.h"
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "ae.h"
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



void client_read(ae_event_loop_t* el, int fd, void* privdata, int mask) {
    thread_info* info = (thread_info*)privdata;
    struct conn_info *ci = zmalloc(sizeof(struct conn_info));
    if (!ci) {
        perror("malloc");
        ae_file_event_delete(el, ci->fd, AE_READABLE);
        close(fd);
        return;
    }
    
    ci->fd = fd;
    ssize_t len = 0;
    
       
    len = ae_read(el, ci->fd, ci->buffer, BUFFER_SIZE);
    if (len <= 0) {
        LATTE_LIB_LOG(LOG_DEBUG,"client read error %d", ci->fd);
        ae_file_event_delete(el, ci->fd, AE_READABLE);
        close(ci->fd);
        zfree(ci);
        return;
    }
    
    
    if (ae_write(el, ci->fd, ci->buffer, len) < 0) {
        printf("client write error\n");
        perror("client write");
        return;
    }
    qps_counter++;
    
    zfree(ci);

}
void server_accept(ae_event_loop_t* el, int fd, void* privdata, int mask) {
    LATTE_LIB_LOG(LOG_DEBUG,"server_accept");
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd == -1) {
        perror("accept");
        return;
    }
    fcntl(client_fd, F_SETFL, O_NONBLOCK);
    ae_file_event_new(el, client_fd, AE_READABLE, client_read, privdata);            
}

int server_timer(ae_event_loop_t* el, long long id, void* privdata) {
    printf("QPS: %d\n", qps_counter);
    qps_counter = 0;
    return 1000;
}
// 服务端线程函数
void *server_thread(void *arg) {
    thread_info* info = (thread_info*)arg;
    int server_fd, epoll_fd, timer_fd;
    // struct epoll_event ev, events[MAX_EVENTS];
    ae_event_loop_t* el = ae_event_loop_new(1024);
    struct sockaddr_in addr;

    // 创建服务器socket
    //| SOCK_NONBLOCK
    if ((server_fd = socket(AF_INET, SOCK_STREAM , 0)) == -1) {
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
    
    
    LATTE_LIB_LOG(LOG_DEBUG, "add server_fd %d server_accept event", server_fd);
    ae_file_event_new(el, server_fd, AE_READABLE, server_accept, info);
    
    long long timer_id = ae_time_event_new(el, 1000, server_timer, el, NULL);
    LATTE_LIB_LOG(LOG_DEBUG, "add timeevent %d server_timer event", timer_id);
    


    while (!info->is_stop) {
        ae_process_events(el, AE_ALL_EVENTS|
            AE_CALL_BEFORE_SLEEP|
            AE_CALL_AFTER_SLEEP);
    }

    ae_stop(el);
    ae_event_loop_delete(el);
    return NULL;
}

// 客户端线程函数
void *client_thread(void *arg) {
    thread_info* info = (thread_info*)arg;
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
        
        if (write(sock, buffer, BUFFER_SIZE) < 0) {
            printf("client write error\n");
            perror("client write");
            break;
        }
        
        
        // 接收响应
        if (read(sock, buffer, BUFFER_SIZE) < 0) {
            perror("client read");
            printf("client read error\n");
            break;
        }
        
    }
    
    close(sock);
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
    assert(log_add_stdout(LATTE_LIB, LOG_DEBUG) == 1);
    {
        
        test_cond("io net write", 
            test_server(false, false, false, false) == 1);
        
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}