
#include "async_io.h"
#include <dispatch/dispatch.h>
#include "error/error.h"
#include "thread_single_object/thread_single_object.h"
#define SINGLE_IO_DISPATCH ("async_io_dispatch")
#include "list/list.h"
#include "log/log.h"

typedef struct dispatch_thread_info_t {
    dispatch_queue_t queue;
    list_t* running_requests;
    long long reqs_submitted;
    long long reqs_finished;
} dispatch_thread_info_t;



dispatch_thread_info_t* dispatch_thread_info_new() {
    dispatch_thread_info_t* thread_info = (dispatch_thread_info_t*)zmalloc(sizeof(struct dispatch_thread_info_t));
    thread_info->queue = dispatch_queue_create(SINGLE_IO_DISPATCH, DISPATCH_QUEUE_SERIAL);
    thread_info->reqs_submitted = 0;
    thread_info->reqs_finished = 0;
    thread_info->running_requests = list_new();
    return thread_info;
}

struct dispatch_thread_info_t* get_or_new_thread_info() {
    struct dispatch_thread_info_t* thread_info = (struct dispatch_thread_info_t*)thread_single_object_get(SINGLE_IO_DISPATCH);
    if (thread_info == NULL) {
        thread_info = dispatch_thread_info_new();
        thread_single_object_set(SINGLE_IO_DISPATCH, thread_info);
    }
    return thread_info;
}

// macos 
int request_need_delete(void* value) {
    async_io_request_t* request = (async_io_request_t*)value;
    int need_delete = request->is_finished || request->error != NULL;
    if (need_delete) {
        request->callback(request);
    }
    return need_delete;
}
int async_io_each_finished() {
    struct dispatch_thread_info_t* thread_info = get_or_new_thread_info();
    list_t* running_requests = thread_info->running_requests;
    return list_for_each_delete(running_requests, request_need_delete);
}

int async_io_net_write(async_io_request_t* request)  {
    struct dispatch_thread_info_t* thread_info = get_or_new_thread_info();
    dispatch_queue_t queue = thread_info->queue;
    list_add_node_tail(thread_info->running_requests, request);
    dispatch_async(queue, ^{
        ssize_t result = write(request->fd, request->buf, request->len);
        int success = result > 0;
        if (!success) {
            request->error = error_new(CIOError, "dispatch", "write failed");
        } else {
            request->is_finished = true;
        }
    });
    
    return 0;
}

void dispatch_thread_info_delete(void* arg) {
    struct dispatch_thread_info_t* thread_info = (struct dispatch_thread_info_t*)arg;
    dispatch_release(thread_info->queue);
    list_delete(thread_info->running_requests);
    zfree(thread_info);
}

static bool is_init = false;

int async_io_module_init() {
    if (is_init) {
        return 1;
    }
    thread_single_object_module_init();
    thread_single_object_register(SINGLE_IO_DISPATCH, dispatch_thread_info_delete);
    is_init = true;
    return 1;
}


int async_io_module_destroy() {
    if (!is_init) {
        return 1;
    }
    thread_single_object_delete(SINGLE_IO_DISPATCH);
    thread_single_object_unregister(SINGLE_IO_DISPATCH);
    is_init = false;
    return 1;
}

int async_io_module_thread_init() {    
    return 1;
}

int async_io_module_thread_destroy() {
    thread_single_object_delete(SINGLE_IO_DISPATCH);
    return 1;
}