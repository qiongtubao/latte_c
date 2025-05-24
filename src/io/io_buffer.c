


#include "io.h"

/* ------------------------- Buffer I/O implementation ----------------------- */

/* Returns 1 or 0 for success/failure. */
static size_t ioBufferWrite(io_t *r, const void *buf, size_t len) {
    r->io.buffer.ptr = sds_cat_len(r->io.buffer.ptr,(char*)buf,len);
    r->io.buffer.pos += len;
    return 1;
}

/* Returns 1 or 0 for success/failure. */
static size_t ioBufferRead(io_t *r, void *buf, size_t len) {
    if (sds_len(r->io.buffer.ptr)-r->io.buffer.pos < len)
        return 0; /* not enough buffer to return len bytes. */
    memcpy(buf,r->io.buffer.ptr+r->io.buffer.pos,len);
    r->io.buffer.pos += len;
    return 1;
}

/* Returns read/write position in buffer. */
static off_t ioBufferTell(io_t *r) {
    return r->io.buffer.pos;
}

/* Flushes any buffer to target device if applicable. Returns 1 on success
 * and 0 on failures. */
static int ioBufferFlush(io_t *r) {
    UNUSED(r);
    return 1; /* Nothing to do, our write just appends to the buffer. */
}

static const io_t rioBufferIO = {
    ioBufferRead,
    ioBufferWrite,
    ioBufferTell,
    ioBufferFlush,
    NULL,           /* update_checksum */
    0,              /* current checksum */
    0,              /* flags */
    0,              /* bytes read or written */
    0,              /* read/write chunk size */
    { { NULL, 0 } } /* union for io-specific vars */
};

void io_init_with_buffer(io_t *r, sds s) {
    *r = rioBufferIO;
    r->io.buffer.ptr = s;
    r->io.buffer.pos = 0;
}

