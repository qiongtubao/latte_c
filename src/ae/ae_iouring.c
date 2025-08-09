#include <sys/epoll.h>
#include "ae.h"
#include <liburing.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

/*
    缺点
    1. 如果支持的话 是通过io_uring完成来表示读事件的也就触发事件时已经完成了 会多一次拷贝
    2. 多加一层 read方法  需要使用我们自己的event_fd来处理
*/
typedef enum req_type {
    ADD_EPOLLIN,
    ADD_EPOLLOUT,
    REMOVE_EPOLLIN,
    REMOVE_EPOLLOUT,
} req_type;
typedef struct io_uring_req {
    int fd;
    req_type type;
    char* buffer;
    size_t buffer_size;
    size_t offset;
} io_uring_req;

typedef struct ae_iouring_info {
    int fd;
    int status;                     //0 未使用 1 正在监控可读 2 正在监控可写
    io_uring_req* read_req;
    io_uring_req* write_req;
} ae_iouring_info;

typedef struct ae_api_state_t {
    struct io_uring ring;
    ae_iouring_info* info;
} ae_api_state_t;

void io_uring_req_delete(io_uring_req* req) {
    if (req->buffer != NULL) {
        sds_delete(req->buffer);
    }
    zfree(req);
}
static int ae_api_create(ae_event_loop_t *eventLoop) {
    ae_api_state_t *state = zmalloc(sizeof(ae_api_state_t));
    if (!state) {
        return -1;
    }
    if (io_uring_queue_init(1024, &state->ring, IORING_SETUP_SQPOLL) < 0) {
        zfree(state);
        return -1;
    }
    state->info = zmalloc(eventLoop->setsize * sizeof(ae_iouring_info));
    if (!state->info) {
        zfree(state);
        return -1;
    }
    for (int i = 0; i < eventLoop->setsize; i++) {
        state->info[i].status = 0;
        state->info[i].read_req = NULL;
        state->info[i].write_req = NULL;
    }
    eventLoop->apidata = state;
    return 0;
}
static void ae_api_delete(ae_event_loop_t *eventLoop) {
    ae_api_state_t *state = eventLoop->apidata;
    io_uring_queue_exit(&state->ring);
    for (size_t i = 0; i < eventLoop->setsize; i++)
    {
        if (state->info[i].read_req != NULL) {
            io_uring_req_delete(state->info[i].read_req);
            state->info[i].read_req = NULL;
        }
        if (state->info[i].write_req != NULL) {
            io_uring_req_delete(state->info[i].write_req);
            state->info[i].write_req = NULL;
        }
    }
    
    zfree(state->info);
    zfree(state);
}
static int ae_api_resize(ae_event_loop_t *eventLoop, int setsize) {
    ae_api_state_t *state = eventLoop->apidata;
    state->info = realloc(state->info, setsize * sizeof(ae_iouring_info));
    if (!state->info) {
        return -1;
    }
    for (int i = eventLoop->maxfd + 1; i < setsize; i++) {
        state->info[i].status = 0;
    }
    //暂时不支持resize
    return 0;
}

static int ae_api_add_event(ae_event_loop_t *eventLoop, int fd, int mask) {
    ae_api_state_t *state = eventLoop->apidata;
    if (state->info[fd].status & (1<<0 | 1<<1)) {
        return -1;
    }
    int need_submit = 0;
    if (mask & AE_READABLE && state->info[fd].read_req == NULL) {
        state->info[fd].read_req = zmalloc(sizeof(io_uring_req));
        if (state->info[fd].read_req == NULL) {
            return -1;
        }
        state->info[fd].read_req->fd = fd; 
        state->info[fd].read_req->buffer = sds_new_len(NULL, 16*1024);
        state->info[fd].read_req->buffer_size = 16*1024;
        state->info[fd].read_req->offset = 0;
        struct io_uring_sqe *sqe = io_uring_get_sqe(&state->ring);
        io_uring_prep_poll_add(sqe, fd, EPOLLIN);
        io_uring_sqe_set_data(sqe, (void*)(uintptr_t)state->info[fd].read_req);
        state->info[fd].read_req->type = ADD_EPOLLIN;
        state->info[fd].status |= 1<<0;  
        LATTE_LIB_LOG(LOG_DEBUG,"add read event %d", fd);
        need_submit = 1;
    }
    if (mask & AE_WRITABLE && state->info[fd].write_req == NULL) {
        state->info[fd].write_req = zmalloc(sizeof(io_uring_req));
        if (state->info[fd].write_req == NULL) {
            return -1;
        }
        state->info[fd].write_req->fd = fd;
        state->info[fd].write_req->buffer = sds_new_len(NULL, 16*1024);
        state->info[fd].write_req->buffer_size = 16*1024;
        state->info[fd].write_req->offset = 0;
        struct io_uring_sqe *sqe = io_uring_get_sqe(&state->ring);
        io_uring_prep_poll_add(sqe, fd, EPOLLOUT);
        io_uring_sqe_set_data(sqe, (void*)(uintptr_t)state->info[fd].write_req);
        state->info[fd].write_req->type = ADD_EPOLLOUT;
        state->info[fd].status |= 1<<1;
        //TEST LATTE_LIB_LOG(LOG_DEBUG,"add write event %d", fd);
        need_submit = 1;
    }
    if (need_submit) {
        io_uring_submit(&state->ring);
    }
    return 0;
}
static void ae_api_del_event(ae_event_loop_t *eventLoop, int fd, int delmask) {
    ae_api_state_t *state = eventLoop->apidata;
    if (state->info[fd].status == 0) {
        return;
    }
    int need_submit = 0;
    if (delmask & AE_READABLE) {
        struct io_uring_sqe *sqe = io_uring_get_sqe(&state->ring);
        io_uring_prep_poll_remove(sqe, (void*)(uintptr_t)state->info[fd].read_req);  
        state->info[fd].read_req->type = REMOVE_EPOLLIN;
        state->info[fd].status &= ~(1<<0);
        need_submit = 1;
        LATTE_LIB_LOG(LOG_DEBUG,"remove read event %d", fd);
    }

    if (delmask & AE_WRITABLE) {
        
        struct io_uring_sqe *sqe = io_uring_get_sqe(&state->ring);
        io_uring_prep_poll_remove(sqe, (void*)(uintptr_t)state->info[fd].write_req);
        state->info[fd].write_req->type = REMOVE_EPOLLOUT;
        state->info[fd].status &= ~(1<<1);
        need_submit = 1;
        //TEST LATTE_LIB_LOG(LOG_DEBUG,"remove write event %d", fd);
    }
    if (need_submit) {
        io_uring_submit(&state->ring);
    }
    //是否需要等待完成？  暂定不需要吧
}

static int ae_api_poll(ae_event_loop_t *eventLoop, struct timeval *tvp) {
    unsigned head;
    unsigned count = 0;
    unsigned all = 0;
    ae_api_state_t *state = eventLoop->apidata;
    struct io_uring_cqe* cqe;
    io_uring_for_each_cqe(&state->ring, head, cqe) {
        io_uring_req *req = (io_uring_req *)io_uring_cqe_get_data(cqe);
        
        int mask = 0;
        if (cqe->res >= 0) { // 成功完成
            
            if (req != NULL) {
                //TEST LATTE_LIB_LOG(LOG_DEBUG,"fd = %d,cqe->res = %d, req->type = %d",req->fd, cqe->res, req->type);    
                uint32_t revents = cqe->res; // 获取事件掩码
        
                if (revents & EPOLLIN) {
                    mask |= AE_READABLE;
                    // 处理可读事件
                }

                if (revents & EPOLLOUT) {
                    mask |= AE_WRITABLE;
                    // 处理可写事件
                }
                // 可以检查其他感兴趣的事件标志...
                if (revents & EPOLLERR) mask |= AE_WRITABLE|AE_READABLE;
                if (revents & EPOLLHUP) mask |= AE_WRITABLE|AE_READABLE;
                eventLoop->fired[count].fd = req->fd;
                eventLoop->fired[count].mask = mask;
                if (mask & AE_READABLE) {
                    state->info[req->fd].status &= ~(1<<0);
                }
                if (mask & AE_WRITABLE) {
                    state->info[req->fd].status &= ~(1<<1);
                }
                count++;
            } else {
                LATTE_LIB_LOG(LOG_DEBUG,"req is NULL %d", cqe->res);
            }
            
            
        } else { // 错误发生
            LATTE_LIB_LOG(LOG_DEBUG,"Poll error: %s\n", strerror(-cqe->res));
        }
        all++;
    }
    io_uring_cq_advance(&state->ring, all);
    return count;
}
static char *ae_api_name(void) {
    return "iouring";
}
static int ae_api_read(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len) {    
    return read(fd, buf, buf_len);
}
static int ae_api_write(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len) {
    return write(fd, buf, buf_len);
}

static void ae_api_before_sleep(ae_event_loop_t *eventLoop) {

}

static void ae_api_after_sleep(ae_event_loop_t *eventLoop) {
    ae_api_state_t *state = eventLoop->apidata;
    int need_submit = 0;
    for (int i = 0; i <= eventLoop->maxfd; i++) {
        if (eventLoop->events[i].mask == AE_NONE) {
            continue;
        }
        //LATTE_LIB_LOG(LOG_DEBUG,"fd will add read event %d %d %d", i, eventLoop->events[i].mask & AE_READABLE, !(state->info[i].status & (1<<0)));
        if ((eventLoop->events[i].mask & AE_READABLE) 
                && !(state->info[i].status & (1<<0))) {
            struct io_uring_sqe *sqe = io_uring_get_sqe(&state->ring);
            io_uring_prep_poll_add(sqe, i, EPOLLIN);
            io_uring_sqe_set_data(sqe, (void*)(uintptr_t)state->info[i].read_req);
            
            state->info[i].read_req->type = ADD_EPOLLIN;
            state->info[i].status |= 1<<0;
            
            need_submit = 1;
        }
        if ((eventLoop->events[i].mask & AE_WRITABLE) 
            && !(state->info[i].status & (1<<1))) {
            struct io_uring_sqe *sqe = io_uring_get_sqe(&state->ring);
            io_uring_prep_poll_add(sqe, i, EPOLLOUT);
            io_uring_sqe_set_data(sqe, (void*)(uintptr_t)state->info[i].write_req);
            state->info[i].status |= (1<<1);
            state->info[i].read_req->type = ADD_EPOLLOUT;
            need_submit = 1;    
        }
    }
    if (need_submit) {
        io_uring_submit(&state->ring);
        //TEST LATTE_LIB_LOG(LOG_DEBUG,"io_uring_submit");
    }
    
}