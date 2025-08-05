


#ifdef HAVE_LIBURING
    #include "async_io_iouring.c"
#else
    #include "async_io_dispatch.c"
#endif

#include "async_io.h"
#include "zmalloc/zmalloc.h"

async_io_request_t* async_io_request_new(int fd, char* buf, size_t len, void* ctx, void (*callback)(async_io_request_t* request)) {
    async_io_request_t* request = (async_io_request_t*)zmalloc(sizeof(async_io_request_t));
    request->fd = fd;
    request->buf = buf;
    request->len = len;
    request->offset = 0;
    request->ctx = ctx;
    request->callback = callback;
    request->is_finished = false;
    request->is_canceled = false;
    request->error = NULL;
    return request;
}

async_io_request_t* net_write_request_new(int fd, char* buf, size_t len, void* ctx, void (*callback)( async_io_request_t* request)) {
    async_io_request_t* request = async_io_request_new(fd, buf, len, ctx, callback);
    request->type = ASYNC_IO_NET_WRITE;
    return request;
}

async_io_request_t* net_read_request_new(int fd, char* buf, size_t len, void* ctx, void (*callback)( async_io_request_t* request)) {
    async_io_request_t* request = async_io_request_new(fd, buf, len, ctx, callback);
    request->type = ASYNC_IO_NET_READ;
    return request;
}   

void async_io_request_delete(async_io_request_t* request) {
    if (request->error != NULL && !error_is_ok(request->error)) {
        error_delete(request->error);
        request->error = NULL;
    }
    zfree(request);
}
