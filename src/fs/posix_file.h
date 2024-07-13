#ifndef __LATTE_POSIX_FILE_H
#define __LATTE_POSIX_FILE_H

#include "sds/sds.h"
#include <stdbool.h>
#include <stdlib.h>
#include "utils/error.h"
#include "sds/sds_plugins.h"
#define kWritableFileBufferSize  65536


typedef struct PosixWritableFile {
    int fd;
    sds filename;
    sds dirname;
    size_t pos;
    bool is_manifest;
    char buffer[kWritableFileBufferSize];
} PosixWritableFile;

PosixWritableFile* posixWritableFileCreate(char* filename, int fd);
void posixWritableFileRelease(PosixWritableFile* file);
Error* posixWriteableFileAppend(PosixWritableFile* write, char* data, int size);
Error* posixWritableFileFlush(PosixWritableFile* writer);
Error* posixWritableFileSync(PosixWritableFile* file);
Error* posixWritableFileClose(PosixWritableFile* file);


//============ PosixSequentialFile ============ 
typedef struct PosixSequentialFile {
    sds filename;
    int fd;
} PosixSequentialFile;
Error* posixSequentialFileCreate(sds filename, PosixSequentialFile** fd);
Error* posixSequentialFileRead(PosixSequentialFile* file,size_t n, Slice* result);
Error* posixSequentialFileSkip(PosixSequentialFile* file,uint64_t n);
void posixSequentialFileRelease(PosixSequentialFile* file);
#endif