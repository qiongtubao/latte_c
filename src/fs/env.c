#include "env.h"
#include "file.h"
#include <fcntl.h> // 包含必要的头文件
#include "set/lockSet.h"
#include "posix_file.h"
#include "flags.h"

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

Error* envRemoveFile(Env* env, sds filename) {
    if (removeFile(filename)) {
        return errnoIoCreate(filename);
    }
    return &Ok;
}

Error* envRenameFile(Env* env, sds from, sds to) {
    if (renameFile(from, to)) {
        return errnoIoCreate(from);
    }
    return &Ok;
}

void envWritableFileRelease(Env* env, WritableFile* file) {
    return posixWritableFileRelease(file);
}


//
Error* envDoWriteStringToFile(Env* env, Slice* data,
                                  sds fname, bool should_sync) {
  WritableFile* file;
  Error* error = envNewWritableFile(env, fname, &file);
  if (!isOk(error)) {
    return error;
  }
  error = writableFileAppendSlice(file, data);
  if (isOk(error) && should_sync) {
    error = writableFileSync(file);
  }
  if (isOk(error)) {
    error = writableFileClose(file);
  }
  writableFileRelease(file);  // Will auto-close if we did not close above
  if (!isOk(error)) {
    envRemoveFile(env, fname);
  }
  return error;
}

Error* envWriteStringToFileSync(Env* env, Slice* data,
                             sds fname) {
  return envDoWriteStringToFile(env, data, fname, true);
}

Error* envReadFileToSds(Env* env, sds fname, sds* data) {

    SequentialFile* file;
    Error* error = posixSequentialFileCreate(fname, &file);
    if (!isOk(error)) {
        return error;
    }
    static const int kBufferSize = 8192;
    sds result = sdsnewlen(NULL, kBufferSize);
    Slice buffer = {
        .p = sdsnewlen(NULL, kBufferSize),
        .len = 0
    };
    while (true) {
        Slice fragment;
        error = readSequentialFile(file, kBufferSize, &buffer);
        if (!isOk(error)) {
            break;
        }
        result = sdscatlen(result, buffer.p, buffer.len);
        if (buffer.len == 0) {
            break;
        }
        buffer.len = 0; 
    }
    sdsfree(buffer.p);
    SequentialFileRelease(file);
    return error;


}