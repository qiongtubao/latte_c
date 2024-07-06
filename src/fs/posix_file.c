#include "posix_file.h"

sds baseName(sds filename) {
    size_t pos = sdsFindLastOf(filename, "/");
    if (pos == -1) {
        return sdsnew(filename);
    }
    // assert(find(filename, '/', pos + 1) == C_NPOS);
    return sdsnewlen(filename + pos + 1, 
        sdslen(filename) - pos - 1);
}
bool isManifest(sds filename) {
    sds base = baseName(filename);
    int result = sdsStartsWith(base, "MANIFEST");
    sdsfree(base);
    return result;
}
sds getDirName(sds path) {
    size_t pos = sdsFindLastOf(path, "/");
    if (pos == C_NPOS) {
        return sdsnew(".");
    }
    // assert(find(filename, '/', pos + 1) == C_NPOS);
    return sdsnewlen(path, pos);
}

PosixWritableFile* posixWritableFileCreate(char* filename, int fd) {
    PosixWritableFile* file = zmalloc(sizeof(PosixWritableFile));
    file->filename = sdsnew(filename);
    file->fd = fd;
    file->is_manifest = isManifest(file->filename);
    file->pos = 0;
    file->dirname = getDirName(file->filename);
    file->buffer[kWritableFileBufferSize-1] = '\0';
    return file;
}