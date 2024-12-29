
#ifndef __LATTE_ROCKSDB_H
#define __LATTE_ROCKSDB_H
#include <rocksdb/c.h>
#include "utils/error.h"
#include "sds/sds.h"

typedef struct latte_rocksdb_family_t {
    rocksdb_options_t* cf_opts;
    rocksdb_column_family_handle_t *cf_handle;
    sds cf_name;
} latte_rocksdb_family_t;

typedef struct latte_rocksdb_t {
    rocksdb_options_t* db_opts;
    rocksdb_t* db; 
    int family_count;
    latte_rocksdb_family_t** family_info;
    rocksdb_readoptions_t* ropts;
    rocksdb_writeoptions_t* wopts;
    rocksdb_flushoptions_t* fopts;
    const rocksdb_snapshot_t *snapshot;
    // pthread_rwlock_t rwlock;
} latte_rocksdb_t;

typedef struct latte_rocksdb_query_t {
    rocksdb_column_family_handle_t* cf;
    sds key;
    sds value;
    latte_error_t* error;
} latte_rocksdb_query_t;
latte_rocksdb_t* latte_rocksdb_new();
int latte_rocksdb_read(latte_rocksdb_t* db, size_t count, latte_rocksdb_query_t** querys);
latte_error_t* latte_rocksdb_open(latte_rocksdb_t* rocksdb, char* dir, int count, latte_rocksdb_family_t** family_info);
typedef struct latte_rocksdb_put_t {
    rocksdb_column_family_handle_t* cf;
    sds key;
    sds value;
} latte_rocksdb_put_t;
latte_error_t* latte_rocksdb_write(latte_rocksdb_t* db, size_t count, latte_rocksdb_put_t** puts);

latte_rocksdb_family_t* latte_rocksdb_family_create(latte_rocksdb_t* db, sds cf_name);
latte_rocksdb_put_t* latte_rocksdb_put_new(rocksdb_column_family_handle_t* cf, sds key, sds value);
void latte_rocksdb_put_delete(latte_rocksdb_put_t* put);
latte_rocksdb_query_t* latte_rocksdb_query_new(rocksdb_column_family_handle_t* cf, sds key);
void latte_rocksdb_query_delete(latte_rocksdb_query_t* query);

latte_error_t* latte_rocksdb_flush(latte_rocksdb_t* db);

#endif


