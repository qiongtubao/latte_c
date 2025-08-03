

#ifndef __LATTE_ASYNC_IO_H
#define __LATTE_ASYNC_IO_H

#include <stdbool.h>
#include "error/error.h"


typedef enum async_io_type {
    ASYNC_IO_NET_READ,
    ASYNC_IO_NET_WRITE,
    ASYNC_IO_FILE_READ,
    ASYNC_IO_FILE_WRITE,
} async_io_type;


typedef struct async_io_request_t {
    async_io_type type;
    int fd;
    char* buf;
    size_t len;
    size_t offset;
    void* ctx;
    void (*callback)(struct async_io_request_t* request);
    bool is_finished;
    bool is_canceled;
    latte_error_t* error;
} async_io_request_t;

async_io_request_t* net_write_request_new(int fd, char* buf, size_t len, void* ctx, void (*callback)( async_io_request_t* request));
async_io_request_t* net_read_request_new(int fd, char* buf, size_t len, void* ctx, void (*callback)( async_io_request_t* request));
void async_io_request_delete(async_io_request_t* request);

int async_io_module_init();
int async_io_module_destroy();
int async_io_module_thread_init();
int async_io_module_thread_destroy();


int async_io_net_read(async_io_request_t* request);
int async_io_net_write(async_io_request_t* request);
int async_io_file_read(async_io_request_t* request);
int async_io_file_write(async_io_request_t* request);
int async_io_each_finishd();





#endif