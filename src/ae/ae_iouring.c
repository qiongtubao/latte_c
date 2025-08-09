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
    NONE,
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

io_uring_req* io_uring_req_new(req_type type, int fd) {
    io_uring_req* req = zmalloc(sizeof(io_uring_req));
    if (req == NULL) {
        return NULL;
    }
    req->type = type;
    req->fd = fd;
    req->buffer = NULL;
    req->buffer_size = 0;
    req->offset = 0;
    return req;
}
void io_uring_req_delete(io_uring_req* req) {
    if (req->buffer != NULL) {
        sds_delete(req->buffer);
    }
    zfree(req);
}
static int ae_api_create(ae_event_loop_t *eventLoop) {
    LATTE_LIB_LOG(LOG_DEBUG, "[ae_api_create] ae use iouring");
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
    if (mask & AE_READABLE) {
        if (state->info[fd].read_req == NULL) {
            state->info[fd].read_req = io_uring_req_new(ADD_EPOLLIN, fd);
            if (state->info[fd].read_req == NULL) {
                return -1;
            }
        } else {
            if (state->info[fd].status & (1<<0)) {
                return -1;
            }
            latte_assert(state->info[fd].read_req->type == NONE);
            state->info[fd].read_req->type = ADD_EPOLLIN;
        }
        
        
        struct io_uring_sqe *sqe = io_uring_get_sqe(&state->ring);
        io_uring_prep_poll_add(sqe, fd, EPOLLIN);
        io_uring_sqe_set_data(sqe, (void*)(uintptr_t)state->info[fd].read_req);
        state->info[fd].status |= 1<<0;  
        LATTE_LIB_LOG(LOG_DEBUG,"add read event %d", fd);
        need_submit = 1;
    }
    if (mask & AE_WRITABLE) {
        if (state->info[fd].write_req == NULL) {
            state->info[fd].write_req = io_uring_req_new(ADD_EPOLLOUT, fd);
            if (state->info[fd].write_req == NULL) {
                return -1;
            }
        } else {
            if (state->info[fd].status & (1<<1)) {
                return -1;
            }
            latte_assert(state->info[fd].read_req->type == NONE);
            state->info[fd].write_req->type = ADD_EPOLLOUT;
        }
        
        struct io_uring_sqe *sqe = io_uring_get_sqe(&state->ring);
        io_uring_prep_poll_add(sqe, fd, EPOLLOUT);
        io_uring_sqe_set_data(sqe, (void*)(uintptr_t)state->info[fd].write_req);
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
        io_uring_sqe_set_data(sqe, io_uring_req_new(REMOVE_EPOLLIN, fd));
        state->info[fd].status &= ~(1<<0);
        need_submit = 1;
        //TEST LATTE_LIB_LOG(LOG_DEBUG,"remove read event %d", fd);
    }

    if (delmask & AE_WRITABLE) {
        struct io_uring_sqe *sqe = io_uring_get_sqe(&state->ring);
        io_uring_prep_poll_remove(sqe, (void*)(uintptr_t)state->info[fd].write_req);
        io_uring_sqe_set_data(sqe, io_uring_req_new(REMOVE_EPOLLOUT, fd));
        state->info[fd].status &= ~(1<<1);
        need_submit = 1;
        //TEST LATTE_LIB_LOG(LOG_DEBUG,"remove write event %d", fd);
    }
    if (need_submit) {
        io_uring_submit(&state->ring);
    }
    //是否需要等待完成？  暂定不需要吧 状态是否需要注意一下
    // 取消有好几种情况
    // 1.已经触发了，  此时收到2个回复
    // 2.没有触发，  此时收到1个回复 

}

static int ae_api_poll(ae_event_loop_t *eventLoop, struct timeval *tvp) {
    unsigned head;
    unsigned count = 0;
    unsigned all = 0;
    ae_api_state_t *state = eventLoop->apidata;
    struct io_uring_cqe* cqe;
    io_uring_for_each_cqe(&state->ring, head, cqe) {
        io_uring_req *req = (io_uring_req *)io_uring_cqe_get_data(cqe);
        
        int mask = AE_NONE;
        if (cqe->res >= 0) { // 成功完成
            
            //TEST LATTE_LIB_LOG(LOG_DEBUG,"fd = %d,cqe->res = %d, req->type = %d",req->fd, cqe->res, req->type);    
            uint32_t revents = cqe->res; // 获取事件掩码
            // 可能在删除可读可写事件之前已经有事件触发了  （这里按照当前状态来处理事件是否应该触发）  如果有问题的话以后在修正
            // 可能存在问题就是删除事件后重新添加状态是否正确
            if (revents & EPOLLIN) {
                if (eventLoop->events[req->fd].mask & AE_READABLE) {
                    mask |= AE_READABLE;
                }
                state->info[req->fd].status &= ~(1<<0);
                // 处理可读事件
            }

            if (revents & EPOLLOUT) {
                if (eventLoop->events[req->fd].mask & AE_WRITABLE) {
                    mask |= AE_WRITABLE;
                }
                state->info[req->fd].status &= ~(1<<1);
                // 处理可写事件
            }
            // 可以检查其他感兴趣的事件标志...
            if (revents & EPOLLERR || revents & EPOLLHUP) {
                if (eventLoop->events[req->fd].mask & AE_READABLE) {
                    mask |= AE_READABLE;
                }
                if (eventLoop->events[req->fd].mask & AE_WRITABLE) {
                    mask |= AE_WRITABLE;
                }
                state->info[req->fd].status &= ~(1<<0);
                state->info[req->fd].status &= ~(1<<1);
            }

            if (req->type == REMOVE_EPOLLIN) { //监听可读事件取消成功
                state->info[req->fd].status &= ~(1<<0);
                state->info[req->fd].read_req->type = NONE;
                io_uring_req_delete(req);
            } else if (req->type == REMOVE_EPOLLOUT) { //监听可写事件取消成功
                state->info[req->fd].status &= ~(1<<1);
                state->info[req->fd].write_req->type = NONE;
                io_uring_req_delete(req);
            }

            if (mask  != AE_NONE) {
                eventLoop->fired[count].fd = req->fd;
                eventLoop->fired[count].mask = mask;
                count++;
            }
            
            
        } else { // 错误发生
            
            if (req->type == REMOVE_EPOLLIN) { //监听可读事件取消失败
                state->info[req->fd].read_req->type = NONE;
                io_uring_req_delete(req);
            } else if (req->type == REMOVE_EPOLLOUT) {//监听可写事件取消失败
                state->info[req->fd].write_req->type = NONE;
                io_uring_req_delete(req);
            } else {
                LATTE_LIB_LOG(LOG_DEBUG,"fd(%d) type(%d) Poll error: %s\n", req->fd, req->type,strerror(-cqe->res));
            }
            //可能删除可读可写事件失败
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
    AE_NOTUSED(eventLoop);  
    return read(fd, buf, buf_len);
}
static int ae_api_write(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len) {
    AE_NOTUSED(eventLoop);
    return write(fd, buf, buf_len);
}

static void ae_api_before_sleep(ae_event_loop_t *eventLoop) {
    AE_NOTUSED(eventLoop);
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