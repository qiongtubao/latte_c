#include "env.h"
#include "file.h"
#include <fcntl.h> // 包含必要的头文件
#include "set/lockSet.h"
#include "posix_file.h"

Env* envCreate() {
    Env* env = zmalloc(sizeof(Env));
    lockSetInit(&env->locks_, &sdsSetDictType);
    return env;
}
void envRelease(Env* env) {
    lockSetRelease(&env->locks_);
    zfree(env);
}
//锁定指定文件，避免引发多线程操作对同一个文件的竞争访问
Error* envLockFile(Env* venv, sds filename, FileLock** lock) {
    *lock = NULL;
    int fd = 0;
    Error* error = openFile(filename, &fd, O_RDWR | O_CREAT | kOpenBaseFlags, 0644);
    if (!isOk(error)) {
        return error;
    }
    Env* env = venv;
    if (!lockSetAdd(&env->locks_, filename)) {
        closeFile(fd);
        return ioErrorCreate(filename, "lock file alreay held by process");
    }
    //lock
    if (lockFile(fd) == -1) {
        closeFile(fd);
        lockSetRemove(&env->locks_, filename);
        return errnoIoCreate("lock file %s", filename);
    }
    *lock = fileLockCreate(fd, filename);
    return &Ok;
}

Error* envUnlockFile(Env* venv, FileLock* lock) {
    if (unlockFile(lock->fd) == -1) {
        return errnoIoCreate("unlock %s", lock->filename);
    } 
    Env* env = venv;
    lockSetRemove(&env->locks_, lock->filename);
    closeFile(lock->fd);
    fileLockRelease(lock);
    return &Ok;
}

Error* envNewWritableFile(Env* env, sds filename, WritableFile** file) {
    return newWritableFile(filename, file);
}