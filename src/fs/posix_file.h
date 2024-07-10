#ifndef __LATTE_POSIX_FILE_H
#define __LATTE_POSIX_FILE_H

#include "sds/sds.h"
#include <stdbool.h>
#include <stdlib.h>
#include "utils/error.h"
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
Error* posixWriteableFileAppend(PosixWritableFile* write, char* data, int size);
Error* posixWritableFileFlush(PosixWritableFile* writer);
Error* posixWritableFileSync(PosixWritableFile* file);
Error* posixWritableFileClose(PosixWritableFile* file);
#endif