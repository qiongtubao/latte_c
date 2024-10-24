#include "file.h"
#include <stdbool.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "posix_file.h"
#include "flags.h"
#include <stdio.h>
#if defined(HAVE_O_CLOEXEC)
    int kOpenBaseFlags = O_CLOEXEC;
#else
    int kOpenBaseFlags = 0;
#endif  // defined(HAVE_O_CLOEXEC)
Error* openFile(char* filename, int* fd, int flag, mode_t mode) {
    *fd = open(filename, flag, mode);
    if (*fd < 0) {
        return errnoIoCreate(filename); 
    }
    return &Ok;
}


/**
 * 
fcntl函数中的F_SETLK命令用于对文件进行加锁（锁定）。在Unix和类Unix系统中，文件锁是一种同步机制，用于防止多个进程同时修改同一个文件，从而保证数据的一致性和完整性。
F_SETLK命令允许进程尝试对文件的某一部分施加共享锁（读锁）或排他锁（写锁）。当一个进程持有一个文件的排他锁时，其他进程不能对该文件的同一部分加任何类型的锁，直到原来的锁被释放。共享锁允许多个进程同时读取文件，但不允许任何进程写入文件。
F_SETLK命令的使用通常涉及传递一个struct flock结构体指针作为fcntl函数的第三个参数。struct flock定义了锁的类型、范围和行为。下面是struct flock的定义：
struct flock {
    short l_type;   // Type of lock 
    short l_whence; // How to interpret l_start 
    off_t l_start;  // Offset from l_whence 
    off_t l_len;    // Lock length 
    pid_t l_pid;    // Process ID 
};
l_type字段可以是F_RDLCK（读锁）或F_WRLCK（写锁）。
l_whence字段指定l_start字段的解释方式，通常使用SEEK_SET、SEEK_CUR或SEEK_END。
l_start和l_len字段一起定义了要锁定的文件区域的起始位置和长度。
l_pid字段通常由内核填充，用于存储持有锁的进程ID。
当使用F_SETLK时，进程尝试立即获取锁，如果锁已经被另一个进程持有并且是排他锁，那么F_SETLK会立即失败，并返回一个错误（通常是EWOULDBLOCK）。如果锁是共享锁，并且请求的也是共享锁，那么F_SETLK可能会成功。
如果你想等待锁被释放，可以使用F_SETLKW命令，它会阻塞进程直到锁变为可用。这可以防止死锁的情况，但同时也增加了进程的等待时间。
文件锁是处理并发文件访问的重要工具，特别是在多线程或多进程环境中，它们可以确保文件数据在修改时不会被破坏。不过，使用文件锁时需要小心，避免死锁和资源争用的情况。
 */
int lockOrUnlock(int fd, bool lock) {
  errno = 0;
  struct flock file_lock_info;
  memset(&file_lock_info, 0, sizeof(file_lock_info));
  file_lock_info.l_type = (lock ? F_WRLCK : F_UNLCK);
  file_lock_info.l_whence = SEEK_SET;
  file_lock_info.l_start = 0;
  file_lock_info.l_len = 0;  // Lock/unlock entire file.
  return fcntl(fd, F_SETLK, &file_lock_info);
}

int lockFile(int fd) {
    return lockOrUnlock(fd, true);
}

int unlockFile(int fd) {
    return lockOrUnlock(fd, false);
}

int closeFile(int fd) {
    return close(fd);
}

int removeFile(char* file) {
    return unlink(file);
}

int renameFile(sds_t from, sds_t to) {
    return rename(from, to);
}

FileLock* fileLockCreate(int fd, char* filename) {
    FileLock* fl = zmalloc(sizeof(FileLock));
    fl->fd = fd;
    fl->filename = sds_new(filename);
    return fl;
}

void fileLockRelease(FileLock* lock) {
    sds_delete(lock->filename);
    zfree(lock);
}

//判断某个文件是否存在
bool fileExists(sds_t filename) {
    return access(filename, F_OK) == 0;
}

//创建一个写文件，如果原来有数据则清空数据， 
Error* writableFileCreate(sds_t filename,
                         WritableFile** result)  {
    //O_TRUNC | O_WRONLY | O_CREAT | kOpenBaseFlags: 这是一个位或运算表达式，用于设置open函数的标志参数。这些标志决定了文件的打开模式和行为：
    //O_TRUNC: 如果文件已存在，那么在打开时会被截断至零长度，即清空文件内容。如果文件不存在，此标志将被忽略。
    //O_WRONLY: 请求以只写模式打开文件。如果文件不存在，O_CREAT标志的存在将导致文件被创建。
    //O_CREAT: 请求创建文件，如果文件不存在的话。与O_WRONLY结合使用时，将创建一个可写的空文件。
    //kOpenBaseFlags: 这是一个预定义的宏或变量，它可能包含其他的open标志，具体取决于其定义。例如，它可能包含O_BINARY或O_LARGEFILE等，这取决于你的具体需求和平台支持。
    int fd ;
    Error* error = openFile(filename, &fd,
                    O_TRUNC | O_WRONLY | O_CREAT | kOpenBaseFlags, 0644);
    if (!isOk(error)) {
        return error;
    }
    if (fd < 0) {
      *result = NULL;
      return errnoIoCreate(filename);
    }

    *result = (WritableFile*)posixWritableFileCreate(filename, fd);
    return &Ok;
  }

Error* writableFileAppendSds(WritableFile* file, sds_t data) {
    return writableFileAppend(file, data, sds_len(data));
}  

Error* writableFileAppend(WritableFile* file, char* buf, size_t len) {
    return posixWriteableFileAppend((PosixWritableFile*)file, buf, len);
}

Error* writableFileFlush(WritableFile* file) {
    return posixWritableFileFlush((PosixWritableFile*)file);
}

Error* writableFileSync(WritableFile* file) {
    return posixWritableFileSync((PosixWritableFile*)file);
}

Error* writableFileClose(WritableFile* file) {
    return posixWritableFileClose((PosixWritableFile*)file);
}

Error* writableFileAppendSlice(WritableFile* file, slice_t* data) {
    return writableFileAppend(file, data->p, data->len);
}

void writableFileRelease(WritableFile* file) {
    return posixWritableFileRelease((PosixWritableFile*)file);
}


// ================ SequentialFile =============

//创建顺序读文件
Error* sequentialFileCreate(sds_t filename, SequentialFile** file) {
    return posixSequentialFileCreate(filename, (PosixSequentialFile**)file);
}

//sequentialFile
// 从文件中读取最多“n”个字节。“scratch[0..n-1]”可能
// 由此例程写入。将“*result”设置为已
// 读取的数据（包括成功读取少于“n”个字节的情况）。
// 可以将“*result”设置为指向“scratch[0..n-1]”中的数据，因此
// 使用“*result”时，“scratch[0..n-1]”必须处于活动状态。
// 如果遇到错误，则返回非 OK 状态。
//
// 需要：外部同步(不是线程安全的访问方式)
Error* sequentialFileRead(SequentialFile* file,size_t n, slice_t* result) {
    return posixSequentialFileRead((PosixSequentialFile*)file, n, result);
}

// 从文件中跳过“n”个字节。这保证不会比读取相同数据慢，但可能会更快。
// 如果到达文件末尾，跳过将在文件末尾停止，并且 Skip 将返回 OK。
// 需要：外部同步(不是线程安全的访问方式)
Error* sequentialFileSkip(SequentialFile* file,uint64_t n) {
    return posixSequentialFileSkip((PosixSequentialFile*)file, n);
}

void sequentialFileRelease(SequentialFile* file) {
    posixSequentialFileRelease((PosixSequentialFile*)file);
}

Error* sequentialFileReadSds(SequentialFile* file,size_t n, sds* data) {
    slice_t slice = {
        .p = sds_empty_len(n),
        .len = 0
    };
    Error* error = posixSequentialFileRead((PosixSequentialFile*)file, n, &slice);
    if (!isOk(error)) {
        return error;
    }
    *data = slice.p;
    sds_set_len(slice.p, slice.len);
    return error;
}