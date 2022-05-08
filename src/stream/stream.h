
#include <stdio.h>
#include <stdint.h>
#include <sds.h>

#define STREAM_FLAG_READ_ERROR (1<<0)
#define STREAM_FLAG_WRITE_ERROR (1<<1)

struct _stream {
    size_t (*read)(struct _stream *, void *buf, size_t len);
    size_t (*write)(struct _stream *, const void *buf, size_t len);
    off_t (*tell)(struct _stream *);
    int (*flush)(struct _stream *);
    uint64_t flags;
    size_t processed_bytes;
    size_t max_processing_chunk;
};

typedef struct _stream stream;

size_t streamWrite(stream* s, const void *buf, size_t len);
size_t streamRead(stream* s, void* buf, size_t len);
int streamFlush(stream* s);

// ======================================== about bufferStream 
stream* createBufferStream(sds buffer);
sds getBufferFromBufferStream(stream* s);

// about fileStream
stream* createFileStream(FILE* fp);


//about connStream
// stream* createConnStream();

//about fd
stream* createFdStream(int fd);

//about mmap
stream* createMmapStream(int fd);