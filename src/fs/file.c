#include "file.h"
#include <stdbool.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

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

FileLock* fileLockCreate(int fd, char* filename) {
    FileLock* fl = zmalloc(sizeof(FileLock));
    fl->fd = fd;
    fl->filename = sdsnew(filename);
    return fl;
}

void fileLockRelease(FileLock* lock) {
    sdsfree(lock->filename);
    zfree(lock);
}

//判断某个文件是否存在
bool fileExists(sds filename) {
    return access(filename, F_OK) == 0;
}