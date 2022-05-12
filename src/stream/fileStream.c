#include "stream.h"
#include <zmalloc.h>
#include <util.h>

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

struct fileStream {
    stream s;
    FILE *fp;
    off_t buffered;
    off_t autosync;
} fileStream;

size_t fileStreamWrite(stream* s, const void* buf, size_t len) {
    struct fileStream* fs = (struct fileStream*)s;
    if (!fs->autosync) {
        return fwrite(buf, len, 1, fs->fp);
    }
    size_t nwritten = 0;
    while (len != nwritten) {
        size_t nalign = (size_t)(fs->autosync - fs->buffered);
        size_t towrite = min(nalign , len-nwritten);
        if (fwrite((char*)buf+nwritten,towrite,1,fs->fp) == 0) {
            return 0;
        }
        nwritten += towrite;
        fs->buffered += towrite;
        if (fs->buffered >= fs->autosync) {
            fflush(fs->fp);
            size_t processed = fs->s.processed_bytes + nwritten;
            #if HAVE_SYNC_FILE_RANGE
                if(sync_file_range(fileno(fs->fp),
                    processed - fs->autosync*2,
                    fs->autosync, SYNC_FILE_RANGE_WAIT_BEFORE|
                    SYNC_FILE_RANGE_WRITE|SYNC_FILE_RANGE_WAIT_AFTER) == -1) {
                        return 0;
                }
            #else 
                if (latte_fsync(fileno(fs->fp)) == -1) {
                    return 0;
                }
            #endif 
                fs->buffered = 0;
        }

    }
    return 1;
}

size_t fileStreamRead(stream* s, void* buf, size_t len) {
    struct fileStream* fs = (struct fileStream*)s;
    return fread(buf, len, 1, fs->fp);
}

stream* createFileStream(FILE* fp) {
    struct fileStream* fs = zmalloc(sizeof(struct fileStream));
    fs->s.read = fileStreamRead;
    fs->s.write = fileStreamWrite;
    fs->fp = fp;
    fs->buffered = 0;
    fs->autosync = 0;
    return (stream*)fs;
}