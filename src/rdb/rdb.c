#include "rdb.h"
#include <arpa/inet.h>
#include "zmalloc/zmalloc.h"
#include "crc/crc64.h" /* Disabled for now as file found issues */
#include <string.h>
#include "log/log.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* ---------------- IO Abstractions ---------------- */

#define RDB_LENERR UINT64_MAX
static size_t rdb_file_write(RedisModuleIO *io, const void *buf, size_t len) {
    return fwrite(buf, 1, len, io->io.file.fp);
}

static size_t rdb_file_read(RedisModuleIO *io, void *buf, size_t len) {
    return fread(buf, 1, len, io->io.file.fp);
}

static off_t rdb_file_tell(RedisModuleIO *io) {
    return ftello(io->io.file.fp);
}

static int rdb_file_flush(RedisModuleIO *io) {
    return fflush(io->io.file.fp);
}

void *memcpy(void *dest, const void *src, size_t n);

static size_t rdb_buffer_write(RedisModuleIO *io, const void *buf, size_t len) {
    io->io.buffer.ptr = sds_cat_len(io->io.buffer.ptr, buf, len);
    io->io.buffer.pos += len;
    return len;
}

static size_t rdb_buffer_read(RedisModuleIO *io, void *buf, size_t len) {
    size_t avail = sds_len(io->io.buffer.ptr) - io->io.buffer.pos;
    if (len > avail) len = avail;
    if (len > 0) {
        memcpy(buf, io->io.buffer.ptr + io->io.buffer.pos, len);
        io->io.buffer.pos += len;
    }
    return len;
}

static off_t rdb_buffer_tell(RedisModuleIO *io) {
    return io->io.buffer.pos;
}

static int rdb_buffer_flush(RedisModuleIO *io) {
    return 0;
}

void rdb_init_io_file(RedisModuleIO *io, FILE *fp) {
    io->type = RDB_IO_FILE;
    io->io.file.fp = fp;
    io->io.file.written = 0;
    io->read = rdb_file_read;
    io->write = rdb_file_write;
    io->tell = rdb_file_tell;
    io->flush = rdb_file_flush;
    io->bytes_processed = 0;
    io->checksum = 0;
    io->error = 0;
}

void rdb_init_io_buffer(RedisModuleIO *io, sds buffer) {
    io->type = RDB_IO_BUFFER;
    io->io.buffer.ptr = buffer ? buffer : sds_empty();
    io->io.buffer.pos = 0;
    io->read = rdb_buffer_read;
    io->write = rdb_buffer_write;
    io->tell = rdb_buffer_tell;
    io->flush = rdb_buffer_flush;
    io->bytes_processed = 0;
    io->checksum = 0;
    io->error = 0;
}

sds rdb_get_buffer(RedisModuleIO *io) {
    if (io->type == RDB_IO_BUFFER) {
        return io->io.buffer.ptr;
    }
    return NULL;
}

/* ---------------- Primitives ---------------- */

/* Basic write wrapper that updates checksum and bytes processed */
static ssize_t rdb_write_raw(RedisModuleIO *io, const void *p, size_t len) {
    if (io->error) return -1;
    size_t nwritten = io->write(io, p, len);
    if (nwritten != len) {
        io->error = 1;
        return -1;
    }
    io->bytes_processed += len;
    // io->checksum = crc64(io->checksum, p, len); /* TODO: Enable CRC */
    return len;
}

static ssize_t rdb_read_raw(RedisModuleIO *io, void *p, size_t len) {
    if (io->error) return -1;
    size_t nread = io->read(io, p, len);
    if (nread != len) {
        io->error = 1;
        return -1;
    }
    io->bytes_processed += len;
    // io->checksum = crc64(io->checksum, p, len);
    return len;
}

ssize_t rdb_save_type(RedisModuleIO *io, unsigned char type) {
    return rdb_write_raw(io, &type, 1);
}

int rdb_load_type(RedisModuleIO *io) {
    unsigned char type;
    if (rdb_read_raw(io, &type, 1) == -1) return -1;
    return type;
}

/* Length encoding */
ssize_t rdb_save_len(RedisModuleIO *io, uint64_t len) {
    unsigned char buf[2];
    size_t nwritten;

    if (len < (1<<6)) {
        /* Save a 6 bit len */
        buf[0] = (len&0xFF)|(0<<6);
        if (rdb_write_raw(io, buf, 1) == -1) return -1;
        return 1;
    } else if (len < (1<<14)) {
        /* Save a 14 bit len */
        buf[0] = ((len>>8)&0xFF)|(1<<6);
        buf[1] = len&0xFF;
        if (rdb_write_raw(io, buf, 2) == -1) return -1;
        return 2;
    } else if (len <= UINT32_MAX) {
        /* Save a 32 bit len */
        buf[0] = (2<<6);
        if (rdb_write_raw(io, buf, 1) == -1) return -1;
        uint32_t len32 = htonl(len);
        if (rdb_write_raw(io, &len32, 4) == -1) return -1;
        return 1+4;
    } else {
        /* Save a 64 bit len */
        buf[0] = (3<<6)|0x01; // Special encoding 0x81 for 64bit
        if (rdb_write_raw(io, buf, 1) == -1) return -1;
        uint64_t len64 = htonll(len); /* Need htonll */
        if (rdb_write_raw(io, &len64, 8) == -1) return -1;
        return 1+8;
    }
}

/* Helper for 64bit network order if not available */
#ifndef htonll
uint64_t htonll(uint64_t v) {
    int num = 1;
    if(*(char *)&num == 1) { /* Little Endian */
        union { uint64_t u; uint8_t c[8]; } t = { v };
        return ((uint64_t)t.c[0] << 56) | ((uint64_t)t.c[1] << 48) |
               ((uint64_t)t.c[2] << 40) | ((uint64_t)t.c[3] << 32) |
               ((uint64_t)t.c[4] << 24) | ((uint64_t)t.c[5] << 16) |
               ((uint64_t)t.c[6] << 8)  | ((uint64_t)t.c[7]);
    } else {
        return v;
    }
}
#endif
#ifndef ntohll
uint64_t ntohll(uint64_t v) {
    return htonll(v); // Symmetric
}
#endif


uint64_t rdb_load_len(RedisModuleIO *io, int *isencoded) {
    unsigned char buf[2];
    if (isencoded) *isencoded = 0;
    
    if (rdb_read_raw(io, buf, 1) == -1) return RDB_LENERR;
    int type = (buf[0]&0xC0)>>6;
    
    if (type == 0) {
        return buf[0]&0x3F;
    } else if (type == 1) {
        if (rdb_read_raw(io, buf+1, 1) == -1) return RDB_LENERR;
        return ((buf[0]&0x3F)<<8)|buf[1];
    } else if (type == 2) {
        uint32_t len;
        if (rdb_read_raw(io, &len, 4) == -1) return RDB_LENERR;
        return ntohl(len);
    } else if (type == 3) {
        // Special format
        if (isencoded) *isencoded = 1;
        return buf[0]&0x3F;
    }
    return RDB_LENERR;
}




ssize_t rdb_save_string(RedisModuleIO *io, const char *str, size_t len) {
    ssize_t n1 = rdb_save_len(io, len);
    if (n1 == -1) return -1;
    ssize_t n2 = 0;
    if (len > 0) {
        n2 = rdb_write_raw(io, str, len);
        if (n2 == -1) return -1;
    }
    return n1 + n2;
}

sds rdb_load_string(RedisModuleIO *io) {
    int isencoded;
    uint64_t len = rdb_load_len(io, &isencoded);
    if (isencoded) {
        /* TODO: Handle encoded strings (integer, LZF) */
        // For now we assume we don't use special encodings for simple string save
        return NULL; 
    }
    if (len == RDB_LENERR) return NULL;
    
    sds val = sds_new_len(NULL, len);
    if (len > 0 && rdb_read_raw(io, val, len) == -1) {
        sds_delete(val);
        return NULL;
    }
    return val;
}

/* ---------------- Objects ---------------- */

ssize_t rdb_save_object(RedisModuleIO *io, latte_object_t *o) {
    ssize_t n = 0;
    
    if (o->type == OBJ_STRING) {
        /* Save string */
        n = rdb_save_string(io, o->ptr, sds_len(o->ptr));
    } else if (o->type == OBJ_MODULE) {
        /* Save module object */
        module_value_t *mv = o->ptr;
        module_type_t *mt = mv->type;
        
        /* Write module ID (TODO: proper module ID saving, here we just assume it is known/fixed or save name) */
        /* In real RDB, we save module ID which maps to a module type. */
        
        if (mt->save) {
            mt->save((void*)io, mv->value);
        }
    } else {
        /* TODO: other types */
        return -1;
    }
    return n;
}

latte_object_t *rdb_load_object(RedisModuleIO *io, int type) {
    if (type == OBJ_STRING) {
        sds s = rdb_load_string(io);
        if (!s) return NULL;
        return latte_object_new(OBJ_STRING, s);
    } 
    /* For OBJ_MODULE, we need a way to look up the module type/constructor. 
       Usually this involves reading a module ID first. 
       We need a registry of module types to load properly. 
       This part requires `server` integration to look up modules. 
    */
    return NULL;
}
