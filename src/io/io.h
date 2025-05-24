
#ifndef LATTE_C_IO_H
#define LATTE_C_IO_H

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "sds/sds.h"
#include "connection/connection.h"
#define PROTO_IOBUF_LEN         (1024*16)  /* Generic I/O buffer size */

#define IO_FLAG_READ_ERROR (1<<0)
#define IO_FLAG_WRITE_ERROR (1<<1)
struct io_t {
    size_t (*read)(struct io_t*, void* buf, size_t len);
    size_t (*write)(struct io_t *, const void *buf, size_t len);
    off_t (*tell)(struct io_t *);
    int (*flush)(struct io_t *);
    void (*update_cksum)(struct io_t *, const void *buf, size_t len);
    uint64_t cksum, flags;
    size_t processed_bytes;
    size_t max_processing_chunk;
    union {
        struct {
            sds ptr;
            off_t pos;
        } buffer;

        struct {
            FILE *fp;
            off_t buffered;
            off_t autosync;
        } file;
        struct {
            connection *conn;
            off_t pos;
            sds buf;
            size_t read_limit;
            size_t read_so_far;
        } conn;
        struct {
            int fd;
            off_t pos;
            sds buf;
        } fd;
    } io;
};
typedef struct io_t io_t;
/* The following functions are our interface with the stream. They'll call the
 * actual implementation of read / write / tell, and will update the checksum
 * if needed. */
static inline size_t io_write(io_t *r, void *buf, size_t len) {
    if (r ->flags & IO_FLAG_WRITE_ERROR) return 0;
    while (len) {
        size_t bytes_to_write = (r->max_processing_chunk && r->max_processing_chunk < len) ? r->max_processing_chunk : len;
        if (r->update_cksum) r->update_cksum(r,buf,bytes_to_write);
        if (r->write(r,buf,bytes_to_write) == 0) {
            r->flags |= IO_FLAG_WRITE_ERROR;
            return 0;
        }
        buf = (char*)buf + bytes_to_write;
        len -= bytes_to_write;
        r->processed_bytes += bytes_to_write;
    }
    return 1;
}


static inline size_t io_read(io_t *r, void *buf, size_t len) {
    if (r->flags & IO_FLAG_READ_ERROR) return 0;
    while (len) {
        size_t bytes_to_read = (r->max_processing_chunk && r->max_processing_chunk < len) ? r->max_processing_chunk : len;
        if (r->read(r,buf,bytes_to_read) == 0) {
            r->flags |= IO_FLAG_READ_ERROR;
            return 0;
        }
        if (r->update_cksum) r->update_cksum(r,buf,bytes_to_read);
        buf = (char*)buf + bytes_to_read;
        len -= bytes_to_read;
        r->processed_bytes += bytes_to_read;
    }
    return 1;
}

static inline off_t io_tell(io_t *r) {
    return r->tell(r);
}

static inline int io_flush(io_t *r) {
    return r->flush(r);
}

/* This function allows to know if there was a read error in any past
 * operation, since the rio stream was created or since the last call
 * to rioClearError(). */
static inline int io_get_read_error(io_t *r) {
    return (r->flags & IO_FLAG_READ_ERROR) != 0;
}

/* Like rioGetReadError() but for write errors. */
static inline int io_get_write_error(io_t *r) {
    return (r->flags & IO_FLAG_WRITE_ERROR) != 0;
}

static inline void io_clear_errors(io_t *r) {
    r->flags &= ~(IO_FLAG_READ_ERROR|IO_FLAG_WRITE_ERROR);
}

void io_init_with_file(io_t* r, FILE *fp);
void io_init_with_buffer(io_t* r, sds s);
void io_init_with_conn(io_t* r, connection *coon, size_t read_limit);
void io_init_with_fd(io_t* r, int fd);

void io_free_fd(io_t* r);
void io_free_conn(io_t* r, sds* out_remainingBufferedData);

size_t io_write_bulk_count(io_t* r, char prefix, long count);
size_t io_write_bulk_string(io_t* r, const char * buf, size_t len);
size_t io_write_bulk_longlong(io_t* r, long long l);
size_t io_write_bulk_double(io_t* r, double d);

void io_generic_update_checksum(io_t* r, const void *buf, size_t len);
void io_set_auto_sync(io_t* r, off_t bytes);

int is_io_file(io_t* r);
#endif