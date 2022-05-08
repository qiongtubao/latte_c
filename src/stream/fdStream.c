#include "stream.h"
#include <errno.h>


/* Protocol and I/O related defines */
#define PROTO_IOBUF_LEN         (1024*16)  /* Generic I/O buffer size */
#define PROTO_REPLY_CHUNK_BYTES (16*1024) /* 16k output buffer */
#define PROTO_INLINE_MAX_SIZE   (1024*64) /* Max size of inline reads */
#define PROTO_MBULK_BIG_ARG     (1024*32)
#define PROTO_RESIZE_THRESHOLD  (1024*32) /* Threshold for determining whether to resize query buffer */
#define PROTO_REPLY_MIN_BYTES   (1024) /* the lower limit on reply buffer size */
#define REDIS_AUTOSYNC_BYTES (1024*1024*4) /* Sync file every 4MB. */

struct fdStream {
    stream s;
    int fd;
    int pos;
    sds buf;
} fdStream;


static size_t streamFdWrite(stream* s, const void* buf, size_t len) {
    struct fdStream* fs = (struct fdStream*)s;
    ssize_t retval;
    unsigned char *p = (unsigned char*) buf;
    int doflush = (buf == NULL && len == 0);
    if (len > PROTO_IOBUF_LEN) {
        if (sdslen(fs->buf)) {
            if (streamFdWrite(s, NULL, 0) == 0) {
                return 0;
            }
        }
    } else {
        if (len) {
            fs->buf = sdscatlen(fs->buf, buf, len);
            if (sdslen(fs->buf) > PROTO_IOBUF_LEN) {
                doflush = 1;
            } 
            if (!doflush) {
                return 1;
            }
        }
        p = (unsigned char*) fs->buf;
        len = sdslen(fs->buf);
    }
    size_t nwritten = 0;
    while(nwritten != len) {
        retval = write(fs->fd, p+nwritten, len-nwritten);
        if (retval <= 0) {
            if(retval == -1 && errno == EINTR) continue;
            if (retval == -1 && errno == EWOULDBLOCK) errno = ETIMEDOUT;
            return 0;
        }
        nwritten += retval;
    }
    fs->pos += len;
    sdsclear(fs->buf);
    return 1;
}

size_t streamFdRead(stream* s, void* buf, size_t len) {
    return 0;
}

int streamFdFlush(stream* s) {
    return streamFdWrite(s, NULL, 0);
}

stream* createFdStream(int fd) {
    struct fdStream* fs = zmalloc(sizeof(struct fdStream));
    fs->fd = fd;
    fs->pos = 0;
    fs->buf = sdsempty();
    fs->s.write = streamFdWrite;
    fs->s.read = streamFdRead;
    fs->s.flush = streamFdFlush;
    return fs;
}