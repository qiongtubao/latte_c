

#ifndef __LATTE_ASYNC_IO_H
#define __LATTE_ASYNC_IO_H





typedef struct file_read_request_t {
    int fd;
    
} file_read_request_t;

typedef struct file_write_request_t {
    int fd;
    
} file_write_request_t;

typedef struct net_read_request_t {
    int fd;
    
} net_read_request_t;

typedef struct net_write_request_t {
    int fd;
    
} net_write_request_t;


typedef struct async_io_t {
    
} async_io_t;

int async_io_net_read(async_io_t* async_io, net_read_request_t* request);
int async_io_net_write(async_io_t* async_io, net_write_request_t* request);
int async_io_file_read(async_io_t* async_io, file_read_request_t* request);
int async_io_file_write(async_io_t* async_io, file_write_request_t* request);





#endif