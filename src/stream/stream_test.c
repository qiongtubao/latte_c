#include "test/testhelp.h"
#include "test/testassert.h"
#include <stdio.h>
#include <sds.h>
#include "stream.h"
#include <fcntl.h>
#include <unistd.h>

int test_bufferStream_write() {
    sds buffer;
    stream* s;
    sds b;
    //write
    buffer = sdsempty();
    s = createBufferStream(buffer);
    assert(streamWrite(s, "hello", 5) == 1);
    b = getBufferFromBufferStream(s);
    assert(strcmp(b, "hello") == 0);
    assert(streamWrite(s, "test", 4) == 1);
    b = getBufferFromBufferStream(s);
    assert(strcmp(b, "hellotest") == 0);
    return 1;
}

int test_bufferStream_read() {
    sds buffer;
    stream* s;
    char buf[100];

    buffer = sdsnew("helloworld");
    s = createBufferStream(buffer);
    assert(streamRead(s, buf, 5) == 1);
    assert(strncmp(buf, "hello", 5) == 0);
    return 1;
}

int test_fileStream_write() {
    char* file_name = "./stream_file_write_test.txt";
    FILE *wp = NULL;
    wp = fopen(file_name, "w");
    assert(wp != NULL);
    stream* s = createFileStream(wp);
    assert(streamWrite(s, "hello", 5) == 1);
    FILE* rp = NULL;
    rp = fopen(file_name,"r");
    char buf[100];
    fread(buf, 5, 1, rp);
    assert(strncmp(buf, "hello",5));
    fclose(rp);
    fclose(wp);
    remove(file_name);
    return 1;
}

int test_fileStream_read() {
    char* file_name = "./stream_file_read_test.txt";
    FILE *wp = NULL;
    wp = fopen(file_name, "w");
    assert(wp != NULL);
    fwrite("hello", 5, 1, wp);
    fflush(wp);
    
    FILE* rp = NULL;
    rp = fopen(file_name,"r");
    stream* s = createFileStream(rp);
    char rbuf[100];
    assert(streamRead(s, rbuf, 5) == 1);
    assert(strncmp(rbuf, "hello",5) == 0);
    fclose(rp);
    fclose(wp);
    remove(file_name);
    return 1;
}

int test_fdStream_write() {
    char* file_name = "./stream_fd_write_test.txt";
    int fd=open(file_name,O_CREAT | O_TRUNC | O_RDWR,0666);
    assert(fd >= 0);
    stream* s = createFdStream(fd);
    assert(streamWrite(s, "hello", 5) == 1);
    assert(streamFlush(s) == 1);
    char buf[100];
    int rfd = open(file_name,O_RDONLY,0666);
    int len = read(rfd, buf, 100);
    assert(len == 5);
    assert(strncmp(buf, "hello", 5) == 0);
    close(fd);
    close(rfd);
    remove(file_name);
    return 1;
}

int test_mmapStream_write() {
    char* file_name = "./stream_mmap_write_test.txt";
    int wfd=open(file_name,O_CREAT | O_TRUNC | O_RDWR,0664);
    assert(wfd >= 0);
    
    // assert(ftruncate(fd, 4096) != -1);
    stream* s = createMmapStream(wfd);
    assert(s != NULL);
    assert(streamWrite(s, "hello\0", 6) == 1);
    assert(streamFlush(s) == 1);
    char buf[100];
    int rfd = open(file_name,O_RDONLY,0666);
    //mmap 
    int len = read(rfd, buf, 6);
    assert(len == 6);
    assert(strncmp(buf, "hello", 5) == 0);
    close(rfd);
    close(wfd);
    remove(file_name);
    return 1;
}

int test_mmapStream_read() {
    char* file_name = "./stream_mmap_read_test.txt";
    int wfd = open(file_name,O_CREAT | O_TRUNC | O_RDWR,0666);
    
    write(wfd, "hello", 5);
    
    close(wfd);
    int rfd = open(file_name, O_RDWR,0666);
    
    stream* s = createMmapStream(rfd);
    assert(s != NULL);
    char rbuf[100];
    assert(streamRead(s, rbuf, 5) == 1);
    assert(strncmp(rbuf, "hello",5) == 0);
    close(rfd);
    remove(file_name);
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("bufferStream write function", 
            test_bufferStream_write() == 1);
        test_cond("bufferStream read function",
            test_bufferStream_read() == 1);
        test_cond("fileStream write function",
            test_fileStream_write() == 1);
        test_cond("fileStream read function",
            test_fileStream_read() == 1);
        test_cond("fdStream write function",
            test_fdStream_write() == 1);
        test_cond("mmapStream write function",
            test_mmapStream_write() == 1);
        test_cond("mmapStream read function",
            test_mmapStream_read() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}