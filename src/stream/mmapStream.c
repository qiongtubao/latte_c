#include "stream.h"
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

struct mmapStream {
    stream s;
    int fd;
    void* ptr;
    off_t st_size;
    size_t pos;
} mmapStream;

size_t mmapStreamWrite(stream* s, void* buf, size_t len) {
    struct mmapStream* ms = (struct mmapStream*)s;
    if(ms->st_size - ms->pos < len) {
        return 0;
    }
    memcpy(ms->ptr + ms->pos, buf, len);
    ms->pos += len;
    return 1;
}  

size_t mmapStreamRead(stream* s, void* buf, size_t len) {
    struct mmapStream* ms = (struct mmapStream*)s;
    if(ms->st_size - ms->pos < len) {
        return 0;
    }
    memcpy(buf, ms->ptr + ms->pos, len);
    ms->pos += len;
    return 1;
}


int mmapStreamFlush(stream* s) {
    struct mmapStream* ms = (struct mmapStream*)s;
    if(msync(ms->ptr, ms->st_size, MS_SYNC) ==-1) {
        printf(" errno: %d\n", errno);
        return 0;
    }
    return 1;
}

int get_file_size(int f)
{
  struct stat st;
  fstat(f, &st);
  return st.st_size;
}

stream* createMmapStream(int fd) {
    int len = get_file_size(fd);
    if (len == 0) {
        len = 4096;
        lseek(fd, 4096-1, SEEK_SET);
        write (fd, "\0", 1);
    } 
    void* ptr = mmap(NULL, len,  PROT_READ | PROT_EXEC| PROT_WRITE,  MAP_SHARED, fd, SEEK_SET);
    if (ptr == MAP_FAILED) {
        return NULL;
    }
    struct mmapStream* ms = zmalloc(sizeof(struct mmapStream));
    ms->fd = fd;
    ms->st_size = len;
    ms->ptr = ptr;
    ms->pos = 0;
    ms->s.write = mmapStreamWrite;
    ms->s.read = mmapStreamRead;
    ms->s.flush = mmapStreamFlush;
    return ms;
}


void releaseMmapStream(stream* s) {
    struct mmapStream* ms = (struct mmapStream*)s;
    munmap(ms->ptr, ms->st_size);
    close(ms->fd);
    zfree(ms);
}

