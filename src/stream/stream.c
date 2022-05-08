#include "stream.h"



size_t streamWrite(stream* s, const void *buf, size_t len) {
    if(s->flags & STREAM_FLAG_WRITE_ERROR) return 0;
    while(len) {
        size_t bytes_to_write = (s->max_processing_chunk && s->max_processing_chunk < len)? s->max_processing_chunk: len;
        if (s->write(s, buf, bytes_to_write) == 0) {
            s->flags |= STREAM_FLAG_WRITE_ERROR;
            return 0;
        }
        buf = (char*)buf + bytes_to_write;
        len -= bytes_to_write;
        s->processed_bytes += bytes_to_write;
    }
    return 1;
};


size_t streamRead(stream* s, void* buf, size_t len) {
    if (s->flags & STREAM_FLAG_READ_ERROR) return 0;
    while (len) {
        size_t bytes_to_read = (s->max_processing_chunk && s->max_processing_chunk < len)? s->max_processing_chunk : len;;
        if (s->read(s, buf, bytes_to_read) == 0) {
            s->flags |= STREAM_FLAG_READ_ERROR;
            return 0;
        }
        buf = (char*)buf + bytes_to_read;
        len -= bytes_to_read;
        s->processed_bytes += bytes_to_read;
    }
    return 1;
}

int streamFlush(stream* s) {
    return s->flush(s);
}