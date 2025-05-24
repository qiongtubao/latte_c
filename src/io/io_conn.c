#include "io.h"
#include <errno.h>

/* ------------------- Connection implementation -------------------
 * We use this RIO implementation when reading an RDB file directly from
 * the connection to the memory via rdbLoadRio(), thus this implementation
 * only implements reading from a connection that is, normally,
 * just a socket. */

static size_t ioConnWrite(io_t *r, const void *buf, size_t len) {
    UNUSED(r);
    UNUSED(buf);
    UNUSED(len);
    return 0; /* Error, this target does not yet support writing. */
}

/* Returns 1 or 0 for success/failure. */
static size_t ioConnRead(io_t *r, void *buf, size_t len) {
    size_t avail = sds_len(r->io.conn.buf)-r->io.conn.pos;

    /* If the buffer is too small for the entire request: realloc. */
    if (sds_len(r->io.conn.buf) + sds_avail(r->io.conn.buf) < len)
        r->io.conn.buf = sds_make_room_for(r->io.conn.buf, len - sds_len(r->io.conn.buf));

    /* If the remaining unused buffer is not large enough: memmove so that we
     * can read the rest. */
    if (len > avail && sds_avail(r->io.conn.buf) < len - avail) {
        sds_range(r->io.conn.buf, r->io.conn.pos, -1);
        r->io.conn.pos = 0;
    }

    /* If we don't already have all the data in the sds, read more */
    while (len > sds_len(r->io.conn.buf) - r->io.conn.pos) {
        size_t buffered = sds_len(r->io.conn.buf) - r->io.conn.pos;
        size_t needs = len - buffered;
        /* Read either what's missing, or PROTO_IOBUF_LEN, the bigger of
         * the two. */
        size_t toread = needs < PROTO_IOBUF_LEN ? PROTO_IOBUF_LEN: needs;
        if (toread > sds_avail(r->io.conn.buf)) toread = sds_avail(r->io.conn.buf);
        if (r->io.conn.read_limit != 0 &&
            r->io.conn.read_so_far + buffered + toread > r->io.conn.read_limit)
        {
            /* Make sure the caller didn't request to read past the limit.
             * If they didn't we'll buffer till the limit, if they did, we'll
             * return an error. */
            if (r->io.conn.read_limit >= r->io.conn.read_so_far + len)
                toread = r->io.conn.read_limit - r->io.conn.read_so_far - buffered;
            else {
                errno = EOVERFLOW;
                return 0;
            }
        }
        int retval = connRead(r->io.conn.conn,
                          (char*)r->io.conn.buf + sds_len(r->io.conn.buf),
                          toread);
        if (retval <= 0) {
            if (errno == EWOULDBLOCK) errno = ETIMEDOUT;
            return 0;
        }
        sds_incr_len(r->io.conn.buf, retval);
    }

    memcpy(buf, (char*)r->io.conn.buf + r->io.conn.pos, len);
    r->io.conn.read_so_far += len;
    r->io.conn.pos += len;
    return len;
}

/* Returns read/write position in file. */
static off_t ioConnTell(io_t *r) {
    return r->io.conn.read_so_far;
}

/* Flushes any buffer to target device if applicable. Returns 1 on success
 * and 0 on failures. */
static int ioConnFlush(io_t *r) {
    /* Our flush is implemented by the write method, that recognizes a
     * buffer set to NULL with a count of zero as a flush request. */
    return ioConnWrite(r,NULL,0);
}

static const io_t rioConnIO = {
    ioConnRead,
    ioConnWrite,
    ioConnTell,
    ioConnFlush,
    NULL,           /* update_checksum */
    0,              /* current checksum */
    0,              /* flags */
    0,              /* bytes read or written */
    0,              /* read/write chunk size */
    { { NULL, 0 } } /* union for io-specific vars */
};

/* Create an RIO that implements a buffered read from an fd
 * read_limit argument stops buffering when the reaching the limit. */
void io_init_with_conn(io_t *r, connection *conn, size_t read_limit) {
    *r = rioConnIO;
    r->io.conn.conn = conn;
    r->io.conn.pos = 0;
    r->io.conn.read_limit = read_limit;
    r->io.conn.read_so_far = 0;
    r->io.conn.buf = sds_new_len(NULL, PROTO_IOBUF_LEN);
    sds_clear(r->io.conn.buf);
}

/* Release the RIO stream. Optionally returns the unread buffered data
 * when the SDS pointer 'remaining' is passed. */
void io_free_conn(io_t *r, sds *remaining) {
    if (remaining && (size_t)r->io.conn.pos < sds_len(r->io.conn.buf)) {
        if (r->io.conn.pos > 0) sds_range(r->io.conn.buf, r->io.conn.pos, -1);
        *remaining = r->io.conn.buf;
    } else {
        sds_delete(r->io.conn.buf);
        if (remaining) *remaining = NULL;
    }
    r->io.conn.buf = NULL;
}