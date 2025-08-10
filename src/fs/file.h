#ifndef __LATTE_FILE_H
#define __LATTE_FILE_H

#include "error/error.h"
#include <stdbool.h>
#include "sds/sds_plugins.h"




Error* openFile(char* filename, int* fd, int flag, mode_t mode);
int lockFile(int fd);
int unlockFile(int fd);
typedef struct FileLock {
    int fd;
    sds_t filename;
} FileLock;

typedef struct WritableFile {

} WritableFile;

typedef struct SequentialFile {

} SequentialFile;
FileLock* fileLockCreate(int fd, char* filename);
void fileLockRelease(FileLock* lock);
int removeFile(char* file);
int closeFile(int fd);
bool fileExists(sds_t filename);
int renameFile(sds_t from, sds_t to);

//   ================== WritableFile ==================
Error* writableFileCreate(sds_t filename,
                         WritableFile** result);
void writableFileRelease(WritableFile* file);
Error* writableFileAppendSds(WritableFile* file, sds_t data);
Error* writableFileAppend(WritableFile* file, char* buf, size_t len);
Error* writableFileAppendSlice(WritableFile* file, slice_t* data);
Error* writableFileFlush(WritableFile* file);
Error* writableFileSync(WritableFile* file);
Error* writableFileClose(WritableFile* file);



//   ================== SequentialFile ==================
//创建顺序读文件
Error* sequentialFileCreate(sds_t filename, SequentialFile** fd);
//顺序读（读取文件）
Error* sequentialFileRead(SequentialFile* file,size_t n, slice_t* slice);

Error* sequentialFileReadSds(SequentialFile* file,size_t n, sds* slice);
//顺序读（跳过字节）
Error* sequentialFileSkip(SequentialFile* file,uint64_t n);
//释放
void sequentialFileRelease(SequentialFile* file);
// ============== ==============
#endif