/**
 * @file env.c
 * @brief 环境对象实现
 *        提供文件环境管理，包括文件锁定、文件操作和读写功能
 */
#include "env.h"
#include "file.h"
#include <fcntl.h> /* 包含必要的头文件 */
#include "posix_file.h"
#include "flags.h"
#include "set/hash_set.h"
#define UNUSED(x) (void)(x)

/**
 * @brief 创建环境对象
 * @return 新创建的环境对象指针
 */
Env* envCreate() {
    Env* env = zmalloc(sizeof(Env));
    env->set = hash_set_new(&sds_hash_set_dict_func);
    env->set_lock = latte_mutex_new();
    return env;
}

/**
 * @brief 释放环境对象及其资源
 * @param env 要释放的环境对象指针
 */
void envRelease(Env* env) {
    latte_mutex_lock(env->set_lock);
    set_t* s = env->set;
    env->set = NULL;
    latte_mutex_unlock(env->set_lock);
    set_delete(s);
    latte_mutex_delete(env->set_lock);
    zfree(env);
}

/**
 * @brief 锁定指定文件，避免多线程对同一文件的竞争访问
 * @param venv     环境对象指针
 * @param filename 要锁定的文件名
 * @param lock     输出的文件锁对象指针
 * @return 错误对象指针，成功时返回 &Ok
 */
Error* envLockFile(Env* venv, sds_t filename, FileLock** lock) {
    *lock = NULL;
    int fd = 0;
    Error* error = openFile(filename, &fd, O_RDWR | O_CREAT | kOpenBaseFlags, 0644);
    if (!error_is_ok(error)) {
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
    /* 加锁操作 */
    if (lockFile(fd) == -1) {
        closeFile(fd);
        latte_mutex_lock(env->set_lock);
        set_remove(env->set, filename);
        latte_mutex_unlock(env->set_lock);
        return errno_io_new("lock file %s", filename);
    }
    *lock = fileLockCreate(fd, filename);
    return &Ok;
}

/**
 * @brief 解锁文件
 * @param venv 环境对象指针
 * @param lock 要解锁的文件锁对象
 * @return 错误对象指针，成功时返回 &Ok
 */
Error* envUnlockFile(Env* venv, FileLock* lock) {
    if (unlockFile(lock->fd) == -1) {
        return errno_io_new("unlock %s", lock->filename);
    }
    Env* env = venv;
    latte_mutex_lock(env->set_lock);
    set_remove(env->set, lock->filename);
    latte_mutex_unlock(env->set_lock);
    closeFile(lock->fd);
    fileLockRelease(lock);
    return &Ok;
}

/**
 * @brief 创建可写文件对象
 * @param env      环境对象指针（未使用）
 * @param filename 文件名
 * @param file     输出的可写文件对象指针
 * @return 错误对象指针，成功时返回 &Ok
 */
Error* envWritableFileCreate(Env* env, sds_t filename, WritableFile** file) {
    UNUSED(env);
    return writableFileCreate(filename, file);
}

/**
 * @brief 删除文件
 * @param env      环境对象指针（未使用）
 * @param filename 要删除的文件名
 * @return 错误对象指针，成功时返回 &Ok
 */
Error* envRemoveFile(Env* env, sds_t filename) {
    UNUSED(env);
    if (removeFile(filename)) {
        return errno_io_new(filename);
    }
    return &Ok;
}

/**
 * @brief 重命名文件
 * @param env  环境对象指针（未使用）
 * @param from 源文件名
 * @param to   目标文件名
 * @return 错误对象指针，成功时返回 &Ok
 */
Error* envRenameFile(Env* env, sds_t from, sds_t to) {
    UNUSED(env);
    if (renameFile(from, to)) {
        return errno_io_new(from);
    }
    return &Ok;
}

/**
 * @brief 释放可写文件对象
 * @param env  环境对象指针（未使用）
 * @param file 要释放的可写文件对象
 */
void envWritableFileRelease(Env* env, WritableFile* file) {
    UNUSED(env);
    return posixWritableFileRelease((PosixWritableFile*)file);
}


/**
 * @brief 将 SDS 数据写入文件的内部实现
 * @param env         环境对象指针
 * @param fname       文件名
 * @param data        要写入的数据切片
 * @param should_sync 是否需要同步到磁盘
 * @return 错误对象指针，成功时返回 &Ok
 */
Error* envDoWriteSdsToFile(Env* env, sds_t fname,
                                slice_t* data , bool should_sync) {
  WritableFile* file = NULL;
  Error* error = envWritableFileCreate(env, fname, &file);
  if (!error_is_ok(error)) {
    return error;
  }
  error = writableFileAppendSlice(file, data);
  if (error_is_ok(error) && should_sync) {
    error = writableFileSync(file);
  }
  if (error_is_ok(error)) {
    error = writableFileClose(file);
  }
  /* writableFileRelease(file);  // 如果上面没有关闭会自动关闭 */
  if (!error_is_ok(error)) {
    envRemoveFile(env, fname);
  }
  return error;
}

/**
 * @brief 同步写入 SDS 数据到文件
 * @param env   环境对象指针
 * @param fname 文件名
 * @param data  要写入的 SDS 数据
 * @return 错误对象指针，成功时返回 &Ok
 */
Error* envWriteSdsToFileSync(Env* env,
                             sds_t fname, sds_t data) {
    slice_t slice = {
       .p = data,
       .len = sds_len(data)
    };
  return envDoWriteSdsToFile(env,fname, &slice, true);
}


/**
 * @brief 创建顺序文件对象
 * @param env      环境对象指针（未使用）
 * @param filename 文件名
 * @param file     输出的顺序文件对象指针
 * @return 错误对象指针，成功时返回 &Ok
 */
Error* envSequentialFileCreate(Env* env, sds_t filename, SequentialFile** file) {
    UNUSED(env);
    return posixSequentialFileCreate(filename, (PosixSequentialFile**)file);
}

/**
 * @brief 读取文件内容到 SDS 字符串
 *        分块读取文件内容并拼接成完整的 SDS 字符串
 * @param env   环境对象指针（未使用）
 * @param fname 文件名
 * @param data  输出的 SDS 数据指针
 * @return 错误对象指针，成功时返回 &Ok
 */
Error* envReadFileToSds(Env* env, sds_t fname, sds* data) {
    UNUSED(env);
    SequentialFile* file;
    Error* error = posixSequentialFileCreate(fname, (PosixSequentialFile**)&file);
    if (!error_is_ok(error)) {
        return error;
    }
    static const int kBufferSize = 8192;
    sds_t result = sds_empty_len(kBufferSize);
    /* sds_set_len(result, 0); */
    slice_t buffer = {
        .p = sds_new_len(NULL, kBufferSize),
        .len = 0
    };
    while (true) {
        /* slice_t fragment; */
        error = sequentialFileRead(file, kBufferSize, &buffer);
        if (!error_is_ok(error)) {
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