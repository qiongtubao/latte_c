
#include <liburing.h>
#include "async_io.h"
#include "thread_single_object/thread_single_object.h"
#include "log/log.h"
#include "utils/error.h"
#include <errno.h>
#include "debug/latte_debug.h"
#include "utils/utils.h"
#define SINGLE_IO_URING ("async_io_iouring")
typedef struct io_uring_thread_info_t {
    struct io_uring io_uring_instance;
    long long reqs_submitted;
    long long reqs_finished;
} io_uring_thread_info_t;

bool init_io_uring(struct io_uring* io_uring_instance) {
    int ret = io_uring_queue_init(1024, io_uring_instance,  0);
    if (ret < 0) {
        LATTE_LIB_LOG(LL_ERROR,"io_uring_queue_init failed: %s", strerror(-ret));
        return false;
    }
    return true;
}

io_uring_thread_info_t* io_uring_thread_info_new() {
    io_uring_thread_info_t* thread_info = (io_uring_thread_info_t*)zmalloc(sizeof(struct io_uring_thread_info_t));
    init_io_uring(&thread_info->io_uring_instance);
    thread_info->reqs_submitted = 0;
    thread_info->reqs_finished = 0;
    return thread_info;
}

void io_uring_thread_info_delete(void* arg) {
    struct io_uring_thread_info_t* thread_info = (struct io_uring_thread_info_t*)arg;
    io_uring_queue_exit(&thread_info->io_uring_instance);
    zfree(thread_info);
}

struct io_uring_thread_info_t* get_or_new_thread_info() {
    struct io_uring_thread_info_t* thread_info = (struct io_uring_thread_info_t*)thread_single_object_get(SINGLE_IO_URING);
    if (thread_info == NULL) {
        thread_info = io_uring_thread_info_new();
        thread_single_object_set(SINGLE_IO_URING, thread_info);
    }
    return thread_info;
}



int async_io_net_write(async_io_request_t* request) {
    struct io_uring_thread_info_t* thread_info = get_or_new_thread_info();
    struct io_uring* io_uring_instance = &thread_info->io_uring_instance;
    struct io_uring_sqe* sqe = io_uring_get_sqe(io_uring_instance);
    io_uring_prep_write(sqe, request->fd, request->buf, request->len, 0);
    io_uring_sqe_set_data(sqe, request);
    io_uring_submit(io_uring_instance);
    thread_info->reqs_submitted++;
    return 1;
}

void handle_cqe( struct io_uring_cqe* cqe) {
    async_io_request_t *req = (async_io_request_t *)io_uring_cqe_get_data(cqe);
    latte_assert_with_info(req->is_finished == false, "request is finished");
    switch (req->type)
        {
        case ASYNC_IO_NET_WRITE:
            break;
        case ASYNC_IO_FILE_WRITE:
            break;
        case ASYNC_IO_NET_READ:
            if (cqe->res < 0) {
                fprintf(stderr, "Async read socket failed: %d\n", (cqe->res));
                req->error = error_new(CIOError, "async_io_net_read", "%d",(cqe->res));
            }
        break;
        case ASYNC_IO_FILE_READ:
            if (cqe->res < 0) {
                fprintf(stderr, "Async read file failed: %d\n", (cqe->res));
                req->error = error_new(CIOError, "async_io_net_read", "%d", (cqe->res));
            }
            break;
    default:
        break;
    }
    latte_assert_with_info(req->callback != NULL, "callback is NULL");
    req->is_finished = true;
    req->callback(req);
    
}


int async_io_each_finished() {
    struct io_uring_thread_info_t* thread_info = get_or_new_thread_info();
    if (thread_info->reqs_submitted == thread_info->reqs_finished) {
        return 0;
    }
    struct io_uring* io_uring_instance = &thread_info->io_uring_instance;

    struct io_uring_cqe* cqe;
    unsigned head;
    unsigned count = 0;
    
    io_uring_for_each_cqe(io_uring_instance, head, cqe) {
        if (cqe->res < 0) {
            fprintf(stderr, "Async write failed: %d\n", (cqe->res));
        }
        handle_cqe(cqe);
        count++;
        
    }
    io_uring_cq_advance(io_uring_instance, count);
    thread_info->reqs_finished += count;
    return count;
}

static bool is_init = false;

int async_io_module_init() {
    thread_single_object_module_init();
    thread_single_object_register(SINGLE_IO_URING, io_uring_thread_info_delete);
    return 1;
}


int async_io_module_destroy() {
    thread_single_object_delete(SINGLE_IO_URING);
    thread_single_object_unregister(SINGLE_IO_URING);
    return 1;
}

int async_io_module_thread_init() {
    
    return 1;
}

int async_io_module_thread_destroy() {
    thread_single_object_delete(SINGLE_IO_URING);
    return 1;
}