#ifndef __LATTE_FILE_H
#define __LATTE_FILE_H

#include <utils/error.h>
#include <stdbool.h>




Error* openFile(char* filename, int* fd, int flag, mode_t mode);
int lockFile(int fd);
int unlockFile(int fd);
typedef struct FileLock {
    int fd;
    sds filename;
} FileLock;

typedef struct WritableFile {

} WritableFile;
FileLock* fileLockCreate(int fd, char* filename);
void fileLockRelease(FileLock* lock);

int closeFile(int fd);
bool fileExists(sds filename);

Error* newWritableFile(sds filename,
                         WritableFile** result);
Error* writableFileAppendSds(WritableFile* file, sds data);
Error* writableFileAppend(WritableFile* file, char* buf, size_t len);
Error* writableFileFlush(WritableFile* file);
Error* writableFileSync(WritableFile* file);
Error* writableFileClose(WritableFile* file);
#endif