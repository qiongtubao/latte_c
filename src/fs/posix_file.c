#include "posix_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include "flags.h"
sds_t baseName(sds_t filename) {
    size_t pos = sds_find_lastof(filename, "/");
    if (pos == -1) {
        return sds_new(filename);
    }
    // assert(find(filename, '/', pos + 1) == C_NPOS);
    return sds_new_len(filename + pos + 1, 
        sds_len(filename) - pos - 1);
}
bool isManifest(sds_t filename) {
    sds_t base = baseName(filename);
    int result = sds_starts_with(base, "MANIFEST");
    sds_free(base);
    return result;
}
sds_t getDirName(sds_t path) {
    size_t pos = sds_find_lastof(path, "/");
    if (pos == C_NPOS) {
        return sds_new(".");
    }
    // assert(find(filename, '/', pos + 1) == C_NPOS);
    return sds_new_len(path, pos);
}

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

Error* writeUnbuffered(PosixWritableFile* writer, const char* data, size_t size) {
    while (size > 0) {
      ssize_t write_result = write(writer->fd, data, size);
      if (write_result < 0) {
        if (errno == EINTR) {
          continue;  // Retry
        }
        return errnoIoCreate(writer->filename);
      }
      data += write_result;
      size -= write_result;
    }
    return &Ok;
}


Error* posixWritableFileFlushBuffer(PosixWritableFile* writer) {
    Error* status = writeUnbuffered(writer, writer->buffer, writer->pos);
    //无论写入文件成功或者失败  pos都清空
    writer->pos = 0;
    return status;
}

#define min(a, b)	(a) < (b) ? a : b
Error* posixWriteableFileAppend(PosixWritableFile* writer, char* data, int size) {
    size_t write_size = size;;
    const char* write_data = data;

    // Fit as much as possible into buffer.
    //buffer最大65536 （64k）
    size_t copy_size = min(write_size, kWritableFileBufferSize - writer->pos);
    memcpy(writer->buffer + writer->pos, write_data, copy_size);
    write_data += copy_size;
    write_size -= copy_size;
    writer->pos += copy_size;
    //数据写入buffer 
    if (write_size == 0) {
      return &Ok;
    }

    // Can't fit in buffer, so need to do at least one write.
    // buffer满了 先write到文件
    Error* error = posixWritableFileFlushBuffer(writer);
    if (!isOk(error)) {
      return error;
    }

    // Small writes go to buffer, large writes are written directly.
    //剩下数据小于buffer最大大小（64k） 就存入buffer
    if (writer < kWritableFileBufferSize) {
      memcpy(writer->buffer, write_data, write_size);
      writer->pos = write_size;
      return &Ok;
    }
    //直接写到文件
    return writeUnbuffered(writer,write_data, write_size);
}



Error* posixWritableFileFlush(PosixWritableFile* writer) {
    //缓存写入文件;
    return posixWritableFileFlushBuffer(writer);
}

Error* posixWritableFileSyncFd(int fd, const sds_t fd_path) {
#if HAVE_FULLFSYNC
    // On macOS and iOS, fsync() doesn't guarantee durability past power
    // failures. fcntl(F_FULLFSYNC) is required for that purpose. Some
    // filesystems don't support fcntl(F_FULLFSYNC), and require a fallback to
    // fsync().
    // 在 macOS 和 iOS 上，fsync() 无法保证电源故障后的耐用性。为此需要 fcntl(F_FULLFSYNC)。某些
    // 文件系统不支持 fcntl(F_FULLFSYNC)，需要回退到
    // fsync()。
    if (::fcntl(fd, F_FULLFSYNC) == 0) {
      return Status::OK();
    }
#endif  // HAVE_FULLFSYNC

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
#endif  // HAVE_FDATASYNC

    if (sync_success) {
      return &Ok;
    }
    return errnoIoCreate(fd_path);
}

Error* posixWritableFileSyncDirIfManifest(PosixWritableFile* writer) {
    Error* status;
    if (!writer->is_manifest) {
      return &Ok;
    }

    int fd = open(writer->dirname, O_RDONLY | kOpenBaseFlags);
    if (fd < 0) {
      status = errnoIoCreate(writer->dirname);
    } else {
      status = posixWritableFileSyncFd(fd, writer->dirname);
      close(fd);
    }
    return status;
  }

Error* posixWritableFileSync(PosixWritableFile* file) {
    // Ensure new files referred to by the manifest are in the filesystem.
    //
    // This needs to happen before the manifest file is flushed to disk, to
    // avoid crashing in a state where the manifest refers to files that are not
    // yet on disk.
    // 确保清单引用的新文件位于文件系统中。
    //
    // 这需要在清单文件刷新到磁盘之前发生，以
    // 避免在清单引用尚未在磁盘上的文件的状态下崩溃。
    Error* error = posixWritableFileSyncDirIfManifest(file);
    if (!isOk(error)) {
      return error;
    }

    error = posixWritableFileFlushBuffer(file);
    if (!isOk(error)) {
      return error;
    }

    return posixWritableFileSyncFd(file->fd, file->filename);
}

Error* posixWritableFileClose(PosixWritableFile* file) {
    //缓存写入 写入文件
    Error* error = posixWritableFileFlushBuffer(file);
    //关闭fd
    const int close_result = close(file->fd);
    if (close_result < 0 && isOk(error)) {
      error = errnoIoCreate(file->filename);
    }
    file->fd = -1;
    return error;
}

void posixWritableFileRelease(PosixWritableFile* file) {
  if (file->fd >= 0) {
    posixWritableFileClose(file);
  }
  sds_free(file->dirname);
  sds_free(file->filename);
  zfree(file);
}

//============ posixSequentialFile ============ 
Error* posixSequentialFileCreate(sds_t filename, PosixSequentialFile** file) {
    int fd = open(filename, O_RDONLY | kOpenBaseFlags);
    if (fd < 0) {
      *file = NULL;
      return errnoIoCreate(filename);
    }
    PosixSequentialFile* seq = zmalloc(sizeof(PosixSequentialFile));
    seq->filename = filename;
    seq->fd =fd;
    *file = seq;
    return &Ok;
}

Error* posixSequentialFileRead(PosixSequentialFile* file,size_t n, slice_t* slice) {
  Error* error = &Ok;
  while (true) {
    //read适合顺序读
    ssize_t read_size = read(file->fd, slice->p + slice->len, n);
    if (read_size < 0) {
      if (errno == EINTR) {
        continue; //Retry
      }
      error = errnoIoCreate(file->filename);
      break;
    }
    slice->len += read_size;
    break;
  }
  return error;
}

Error* posixSequentialFileSkip(PosixSequentialFile* file,uint64_t n) {
  if (lseek(file->fd, n, SEEK_CUR) == (off_t)(-1)) {
    return errnoIoCreate(file->filename);
  }
  return &Ok;
}

void posixSequentialFileRelease(PosixSequentialFile* file) {
  close(file->fd);
  zfree(file);
}