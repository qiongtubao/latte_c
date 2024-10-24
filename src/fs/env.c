#include "env.h"
#include "file.h"
#include <fcntl.h> // 包含必要的头文件
#include "posix_file.h"
#include "flags.h"
#include "set/hash_set.h"
#define UNUSED(x) (void)(x)

Env* envCreate() {
    Env* env = zmalloc(sizeof(Env));
    env->set = hash_set_new(&sds_hash_set_dict_func);
    env->set_lock = latte_mutex_new();
    return env;
}
void envRelease(Env* env) {
    latte_mutex_lock(env->set_lock);
    set_t* s = env->set;
    env->set = NULL;
    latte_mutex_unlock(env->set_lock);
    set_delete(s);
    latte_mutex_delete(env->set_lock);
    zfree(env);
}
//锁定指定文件，避免引发多线程操作对同一个文件的竞争访问
Error* envLockFile(Env* venv, sds_t filename, FileLock** lock) {
    *lock = NULL;
    int fd = 0;
    Error* error = openFile(filename, &fd, O_RDWR | O_CREAT | kOpenBaseFlags, 0644);
    if (!isOk(error)) {
        return error;
    }
    Env* env = venv;
    latte_mutex_lock(env->set_lock);
    int result = set_add(env->set, filename);
    latte_mutex_unlock(env->set_lock);
    if (!result) {
        closeFile(fd);
        return ioErrorCreate(filename, "lock file alreay held by process");
    }
    //lock
    if (lockFile(fd) == -1) {
        closeFile(fd);
        latte_mutex_lock(env->set_lock);
        set_remove(env->set, filename);
        latte_mutex_unlock(env->set_lock);
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
    latte_mutex_lock(env->set_lock);
    set_remove(env->set, lock->filename);
    latte_mutex_unlock(env->set_lock);
    closeFile(lock->fd);
    fileLockRelease(lock);
    return &Ok;
}

Error* envWritableFileCreate(Env* env, sds_t filename, WritableFile** file) {
    UNUSED(env);
    return writableFileCreate(filename, file);
}

Error* envRemoveFile(Env* env, sds_t filename) {
    UNUSED(env);
    if (removeFile(filename)) {
        return errnoIoCreate(filename);
    }
    return &Ok;
}

Error* envRenameFile(Env* env, sds_t from, sds_t to) {
    UNUSED(env);
    if (renameFile(from, to)) {
        return errnoIoCreate(from);
    }
    return &Ok;
}

void envWritableFileRelease(Env* env, WritableFile* file) {
    UNUSED(env);
    return posixWritableFileRelease((PosixWritableFile*)file);
}


//
Error* envDoWriteSdsToFile(Env* env, sds_t fname,
                                slice_t* data , bool should_sync) {
  WritableFile* file = NULL;
  Error* error = envWritableFileCreate(env, fname, &file);
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
  //writableFileRelease(file);  // Will auto-close if we did not close above
  if (!isOk(error)) {
    envRemoveFile(env, fname);
  }
  return error;
}

Error* envWriteSdsToFileSync(Env* env, 
                             sds_t fname, sds_t data) {
    slice_t slice = {
       .p = data,
       .len = sds_len(data) 
    };
  return envDoWriteSdsToFile(env,fname, &slice, true);
}


Error* envSequentialFileCreate(Env* env, sds_t filename, SequentialFile** file) {
    UNUSED(env);
    return posixSequentialFileCreate(filename, (PosixSequentialFile**)file);
}

Error* envReadFileToSds(Env* env, sds_t fname, sds* data) {
    UNUSED(env);
    SequentialFile* file;
    Error* error = posixSequentialFileCreate(fname, (PosixSequentialFile**)&file);
    if (!isOk(error)) {
        return error;
    }
    static const int kBufferSize = 8192;
    sds_t result = sds_empty_len(kBufferSize);
    // sds_set_len(result, 0);
    slice_t buffer = {
        .p = sds_new_len(NULL, kBufferSize),
        .len = 0
    };
    while (true) {
        // slice_t fragment;
        error = sequentialFileRead(file, kBufferSize, &buffer);
        if (!isOk(error)) {
            break;
        }
        if (buffer.len == 0) {
            *data = result;
            break;
        }
        result = sds_cat_len(result, buffer.p, buffer.len);
        buffer.len = 0; 
    }
    sds_delete(buffer.p);
    sequentialFileRelease(file);
    return error;


}