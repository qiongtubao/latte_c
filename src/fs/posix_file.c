/**
 * @file posix_file.c
 * @brief POSIX 文件系统实现
 *        提供跨平台的文件操作接口，包括可写文件和顺序读文件的实现
 */
#include "posix_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include "flags.h"

/**
 * @brief 提取文件名的基本名称（去除路径）
 * @param filename 完整文件路径
 * @return 基本文件名的 SDS 字符串
 */
sds_t baseName(sds_t filename) {
    size_t pos = sds_find_lastof(filename, "/");
    if (pos == C_NPOS) {
        return sds_new(filename);
    }
    /* assert(find(filename, '/', pos + 1) == C_NPOS); */
    return sds_new_len(filename + pos + 1,
        sds_len(filename) - pos - 1);
}

/**
 * @brief 检查文件名是否为 MANIFEST 文件
 * @param filename 文件名
 * @return true 表示是 MANIFEST 文件，false 表示不是
 */
bool isManifest(sds_t filename) {
    sds_t base = baseName(filename);
    int result = sds_starts_with(base, "MANIFEST");
    sds_delete(base);
    return result;
}

/**
 * @brief 获取路径的目录名
 * @param path 文件路径
 * @return 目录名的 SDS 字符串
 */
sds_t getDirName(sds_t path) {
    size_t pos = sds_find_lastof(path, "/");
    if (pos == C_NPOS) {
        return sds_new(".");
    }
    /* assert(find(filename, '/', pos + 1) == C_NPOS); */
    return sds_new_len(path, pos);
}

/**
 * @brief 创建 POSIX 可写文件对象
 * @param filename 文件名
 * @param fd       文件描述符
 * @return 新创建的可写文件对象指针
 */
PosixWritableFile* posixWritableFileCreate(char* filename, int fd) {
    PosixWritableFile* file = zmalloc(sizeof(PosixWritableFile));
    file->filename = sds_new(filename);
    file->fd = fd;
    file->is_manifest = isManifest(file->filename);
    file->pos = 0;
    file->dirname = getDirName(file->filename);
    file->buffer[kWritableFileBufferSize-1] = '\0';
    return file;
}

/**
 * @brief 无缓冲写入数据到文件
 *        直接写入数据到文件描述符，处理中断重试
 * @param writer 可写文件对象
 * @param data   要写入的数据
 * @param size   数据大小
 * @return 错误对象指针，成功时返回 &Ok
 */
Error* writeUnbuffered(PosixWritableFile* writer, const char* data, size_t size) {
    while (size > 0) {
      ssize_t write_result = write(writer->fd, data, size);
      if (write_result < 0) {
        if (errno == EINTR) {
          continue;  /* 重试 */
        }
        return errno_io_new(writer->filename);
      }
      data += write_result;
      size -= write_result;
    }
    return &Ok;
}


/**
 * @brief 刷新缓冲区数据到文件
 *        将缓冲区中的所有数据写入文件，无论成功失败都清空缓冲区位置
 * @param writer 可写文件对象
 * @return 错误对象指针，成功时返回 &Ok
 */
Error* posixWritableFileFlushBuffer(PosixWritableFile* writer) {
    Error* status = writeUnbuffered(writer, writer->buffer, writer->pos);
    /* 无论写入文件成功或者失败 pos 都清空 */
    writer->pos = 0;
    return status;
}

#define min(a, b)	(a) < (b) ? a : b

/**
 * @brief 向可写文件追加数据
 *        优先使用缓冲区，缓冲区满时刷新到文件，大数据直接写入
 * @param writer 可写文件对象
 * @param data   要写入的数据
 * @param size   数据大小
 * @return 错误对象指针，成功时返回 &Ok
 */
Error* posixWriteableFileAppend(PosixWritableFile* writer, char* data, int size) {
    size_t write_size = size;;
    const char* write_data = data;

    /* 尽可能多地放入缓冲区 */
    /* buffer 最大 65536 (64k) */
    size_t copy_size = min(write_size, kWritableFileBufferSize - writer->pos);
    memcpy(writer->buffer + writer->pos, write_data, copy_size);
    write_data += copy_size;
    write_size -= copy_size;
    writer->pos += copy_size;
    /* 数据写入 buffer */
    if (write_size == 0) {
      return &Ok;
    }

    /* 无法放入缓冲区，需要至少进行一次写操作 */
    /* buffer 满了先 write 到文件 */
    Error* error = posixWritableFileFlushBuffer(writer);
    if (!error_is_ok(error)) {
      return error;
    }

    /* 小写入放入缓冲区，大写入直接写入文件 */
    /* 剩下数据小于 buffer 最大大小（64k）就存入 buffer */
    if (write_size < kWritableFileBufferSize) {
      memcpy(writer->buffer, write_data, write_size);
      writer->pos = write_size;
      return &Ok;
    }
    /* 直接写到文件 */
    return writeUnbuffered(writer,write_data, write_size);
}



/**
 * @brief 刷新可写文件缓冲区
 *        将缓冲区数据写入文件
 * @param writer 可写文件对象
 * @return 错误对象指针，成功时返回 &Ok
 */
Error* posixWritableFileFlush(PosixWritableFile* writer) {
    /* 缓存写入文件; */
    return posixWritableFileFlushBuffer(writer);
}

/**
 * @brief 同步文件描述符到磁盘
 *        根据不同平台选择最适合的同步方法
 * @param fd      文件描述符
 * @param fd_path 文件路径（用于错误报告）
 * @return 错误对象指针，成功时返回 &Ok
 */
Error* posixWritableFileSyncFd(int fd, const sds_t fd_path) {
#if HAVE_FULLFSYNC
    /* 在 macOS 和 iOS 上，fsync() 无法保证电源故障后的持久性。为此需要 fcntl(F_FULLFSYNC)。某些
     * 文件系统不支持 fcntl(F_FULLFSYNC)，需要回退到 fsync()。 */
    if (::fcntl(fd, F_FULLFSYNC) == 0) {
      return Status::OK();
    }
#endif  /* HAVE_FULLFSYNC */

#if HAVE_FDATASYNC
    /*
      fdatasync()
      fdatasync()函数与fsync()类似，但它只刷新文件的数据部分，而不包括元数据。
      这意味着fdatasync()通常比fsync()更快，因为它减少了I/O操作的次数，特别是当元数据写入磁盘的成本较高时。
     */
    bool sync_success = ::fdatasync(fd) == 0;
#else
    /*
    fsync()
      fsync()函数的作用是刷新由文件描述符指定的文件的缓存，
      并确保所有修改过的内容都被写入到磁盘上。
      这包括文件的数据以及元数据（metadata），如文件的大小、权限、时间戳等。
      因为元数据通常存储在不同的磁盘位置，
      所以fsync()可能需要进行多次I/O操作来完成。
      这意味着fsync()至少会触发两次磁盘写操作：
      一次是为了写入新数据，
      另一次是为了更新文件的修改时间（mtime）和其他元数据。
    */
    bool sync_success = fsync(fd) == 0;
#endif  /* HAVE_FDATASYNC */

    if (sync_success) {
      return &Ok;
    }
    return errno_io_new(fd_path);
}

/**
 * @brief 如果是 MANIFEST 文件则同步目录
 *        确保 MANIFEST 引用的新文件在文件系统中存在
 * @param writer 可写文件对象
 * @return 错误对象指针，成功时返回 &Ok
 */
Error* posixWritableFileSyncDirIfManifest(PosixWritableFile* writer) {
    Error* status;
    if (!writer->is_manifest) {
      return &Ok;
    }

    int fd = open(writer->dirname, O_RDONLY | kOpenBaseFlags);
    if (fd < 0) {
      status = errno_io_new(writer->dirname);
    } else {
      status = posixWritableFileSyncFd(fd, writer->dirname);
      close(fd);
    }
    return status;
  }

/**
 * @brief 同步可写文件到磁盘
 *        确保所有数据和元数据都写入磁盘
 * @param file 可写文件对象
 * @return 错误对象指针，成功时返回 &Ok
 */
Error* posixWritableFileSync(PosixWritableFile* file) {
    /* 确保清单引用的新文件位于文件系统中。
     *
     * 这需要在清单文件刷新到磁盘之前发生，以
     * 避免在清单引用尚未在磁盘上的文件的状态下崩溃。 */
    Error* error = posixWritableFileSyncDirIfManifest(file);
    if (!error_is_ok(error)) {
      return error;
    }

    error = posixWritableFileFlushBuffer(file);
    if (!error_is_ok(error)) {
      return error;
    }

    return posixWritableFileSyncFd(file->fd, file->filename);
}

/**
 * @brief 关闭可写文件
 *        刷新缓冲区并关闭文件描述符
 * @param file 可写文件对象
 * @return 错误对象指针，成功时返回 &Ok
 */
Error* posixWritableFileClose(PosixWritableFile* file) {
    /* 缓存写入 写入文件 */
    Error* error = posixWritableFileFlushBuffer(file);
    /* 关闭 fd */
    const int close_result = close(file->fd);
    if (close_result < 0 && error_is_ok(error)) {
      error = errno_io_new(file->filename);
    }
    file->fd = -1;
    return error;
}

/**
 * @brief 释放可写文件对象及其资源
 * @param file 要释放的可写文件对象
 */
void posixWritableFileRelease(PosixWritableFile* file) {
  if (file->fd >= 0) {
    posixWritableFileClose(file);
  }
  sds_delete(file->dirname);
  sds_delete(file->filename);
  zfree(file);
}

/* ============ posixSequentialFile ============ */

/**
 * @brief 创建 POSIX 顺序文件对象
 * @param filename 文件名
 * @param file     输出的顺序文件对象指针
 * @return 错误对象指针，成功时返回 &Ok
 */
Error* posixSequentialFileCreate(sds_t filename, PosixSequentialFile** file) {
    int fd = open(filename, O_RDONLY | kOpenBaseFlags);
    if (fd < 0) {
      *file = NULL;
      return errno_io_new(filename);
    }
    PosixSequentialFile* seq = zmalloc(sizeof(PosixSequentialFile));
    seq->filename = filename;
    seq->fd =fd;
    *file = seq;
    return &Ok;
}

/**
 * @brief 从顺序文件读取数据
 *        使用 read 系统调用进行顺序读取，处理中断重试
 * @param file  顺序文件对象
 * @param n     要读取的最大字节数
 * @param slice 输出的数据切片
 * @return 错误对象指针，成功时返回 &Ok
 */
Error* posixSequentialFileRead(PosixSequentialFile* file,size_t n, slice_t* slice) {
  Error* error = &Ok;
  while (true) {
    /* read 适合顺序读 */
    ssize_t read_size = read(file->fd, slice->p + slice->len, n);
    if (read_size < 0) {
      if (errno == EINTR) {
        continue; /* 重试 */
      }
      error = errno_io_new(file->filename);
      break;
    }
    slice->len += read_size;
    break;
  }
  return error;
}

/**
 * @brief 在顺序文件中跳过指定字节数
 * @param file 顺序文件对象
 * @param n    要跳过的字节数
 * @return 错误对象指针，成功时返回 &Ok
 */
Error* posixSequentialFileSkip(PosixSequentialFile* file,uint64_t n) {
  if (lseek(file->fd, n, SEEK_CUR) == (off_t)(-1)) {
    return errno_io_new(file->filename);
  }
  return &Ok;
}

/**
 * @brief 释放顺序文件对象及其资源
 * @param file 要释放的顺序文件对象
 */
void posixSequentialFileRelease(PosixSequentialFile* file) {
  close(file->fd);
  zfree(file);
}