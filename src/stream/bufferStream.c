

#include "stream.h"
#include "zmalloc.h"
#include <string.h>

struct bufferStream {
    stream s;
    size_t pos;
    sds ptr;
} bufferStream;

size_t bufferStreamWrite(stream* s, const void *buf, size_t len) {
    struct bufferStream* bs = (struct bufferStream*)s;
    bs->ptr = sdscatlen(bs->ptr, (char*)buf, len);
    bs->pos += len;
    return 1;
}

size_t bufferStreamRead(stream* s, void* buf, size_t len) {
    struct bufferStream* bs = (struct bufferStream*)s;
    if (sdslen(bs->ptr) - bs->pos < len) {
        return 0;
    }
    memcpy(buf, bs->ptr + bs->pos, len);
    bs->pos += len;
    return 1;
}

stream* createBufferStream(sds buffer) {
    struct bufferStream* s = zmalloc(sizeof(bufferStream));
    s->pos = 0;
    s->ptr = buffer;
    s->s.write = bufferStreamWrite;
    s->s.read = bufferStreamRead;
    return (stream*)s;
}


sds getBufferFromBufferStream(stream* s) {
    struct bufferStream* bs = (struct bufferStream*)s;
    return bs->ptr;
}

