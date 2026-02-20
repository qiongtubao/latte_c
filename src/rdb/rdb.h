#ifndef __LATTE_RDB_H
#define __LATTE_RDB_H

#include <stdio.h>
#include <stdint.h>
#include "sds/sds.h"
#include "object/object.h"
#include "object/module.h"

/* RDB version */
#define RDB_VERSION 1

/* RDB defines */
#define RDB_OPCODE_EOF 255
#define RDB_OPCODE_SELECTDB 254
#define RDB_OPCODE_EXPIRETIME 253
#define RDB_OPCODE_EXPIRETIME_MS 252
#define RDB_OPCODE_RESIZEDB 251
#define RDB_OPCODE_AUX 250
#define RDB_OPCODE_MODULE_AUX 247

/* IO types */
#define RDB_IO_FILE 1
#define RDB_IO_BUFFER 2

/* Forward declaration */
struct RedisModuleIO;

/* Function pointers for IO abstraction */
typedef size_t (*rdb_read_func)(struct RedisModuleIO *io, void *buf, size_t len);
typedef size_t (*rdb_write_func)(struct RedisModuleIO *io, const void *buf, size_t len);
typedef off_t (*rdb_tell_func)(struct RedisModuleIO *io);
typedef int (*rdb_flush_func)(struct RedisModuleIO *io);

typedef struct RedisModuleIO {
    int type;                   /* RDB_IO_FILE or RDB_IO_BUFFER */
    uint64_t bytes_processed;   /* Total bytes read/written */
    uint64_t checksum;          /* Current checksum */
    int error;                  /* Error flag */
    
    /* IO Context */
    union {
        struct {
            FILE *fp;
            off_t written; /* For validation */
        } file;
        struct {
            sds ptr;
            off_t pos;
        } buffer;
    } io;

    /* Virtual methods */
    rdb_read_func read;
    rdb_write_func write;
    rdb_tell_func tell;
    rdb_flush_func flush;
    
    /* Optional: context for module specific saving/loading if needed */
    void *ctx; 
} RedisModuleIO;

/* Initialization */
void rdb_init_io_file(RedisModuleIO *io, FILE *fp);
void rdb_init_io_buffer(RedisModuleIO *io, sds buffer); /* Buffer can be NULL to start empty */
sds rdb_get_buffer(RedisModuleIO *io);

/* Primitive RDB operations */
ssize_t rdb_save_type(RedisModuleIO *io, unsigned char type);
ssize_t rdb_save_len(RedisModuleIO *io, uint64_t len);
ssize_t rdb_save_string(RedisModuleIO *io, const char *str, size_t len);
ssize_t rdb_save_binary_string(RedisModuleIO *io, const unsigned char *s, size_t len);
ssize_t rdb_save_long_long_as_string(RedisModuleIO *io, long long value);
ssize_t rdb_save_object(RedisModuleIO *io, latte_object_t *o);

int rdb_load_type(RedisModuleIO *io);
uint64_t rdb_load_len(RedisModuleIO *io, int *isencoded);
sds rdb_load_string(RedisModuleIO *io);
latte_object_t *rdb_load_object(RedisModuleIO *io, int type);

/* High level API */
int rdb_save(const char *filename,  latte_object_t *obj); /* Example convenience struct */
// int rdb_load(const char *filename, ...); 

#endif
