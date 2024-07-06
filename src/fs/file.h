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
FileLock* fileLockCreate(int fd, char* filename);
void fileLockRelease(FileLock* lock);

int closeFile(int fd);
bool fileExists(sds filename);

#endif