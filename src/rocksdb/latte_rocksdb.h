#ifndef __LATTE_ROCKSDB_H__
#define __LATTE_ROCKSDB_H__   


#include <rocksdb/c.h>
#include "dict/dict.h"
#include "sds/sds.h"
#include "error/error.h"
#include "dict/dict_plugins.h"

typedef struct latte_rocksdb_t {
    sds db_path;
    rocksdb_t* db;
    rocksdb_options_t* db_opts;
    dict_t* column_families_metas;
    rocksdb_writeoptions_t* write_opts;
    rocksdb_readoptions_t* read_opts;
    rocksdb_compactoptions_t* compact_opts;
    rocksdb_flushoptions_t* flush_opts;
} latte_rocksdb_t;

typedef struct latte_rocksdb_column_family_meta_t {
    sds name;
    rocksdb_options_t* cf_opts;
    rocksdb_column_family_handle_t* handle;
} latte_rocksdb_column_family_meta_t;


#define LATTE_SET_DB_OPTION(rocksdb, option, ...) \
    do { \
        rocksdb_options_##option(rocksdb->db_opts, __VA_ARGS__); \
    } while (0)

#define LATTE_SET_DB_READ_OPTION(rocksdb, option, ...) \
    do { \
        rocksdb_readoptions_##option(rocksdb->read_opts, __VA_ARGS__); \
    } while (0)

#define LATTE_SET_DB_WRITE_OPTION(rocksdb, option, ...) \
    do { \
        rocksdb_writeoptions_##option(rocksdb->write_opts, __VA_ARGS__); \
    } while (0)

#define LATTE_ROCKSDB_SET_CF_OPTION(meta, option,  ...) \
    do { \
        rocksdb_options_##option(meta->cf_opts, __VA_ARGS__); \
    } while (0)

#define LATTE_ROCKSDB_SET_CF_OPTION_BY_NAME(rocksdb, name, option, ...) \
    do { \
        sds _name = sds_new(name); \
        dict_entry_t* _entry = dict_find(rocksdb->column_families_metas, _name); \
        if (_entry) { \
            latte_rocksdb_column_family_meta_t* _meta = dict_get_val(_entry); \
            rocksdb_options_set_##option(_meta->cf_opts, __VA_ARGS__); \
        } \
        sds_free(_name); \
    } while (0)


latte_rocksdb_t* latte_rocksdb_create(sds dir_name);


latte_error_t* latte_rocksdb_open(latte_rocksdb_t* db);

void latte_rocksdb_close(latte_rocksdb_t* db);

latte_rocksdb_column_family_meta_t* latte_rocksdb_add_column_family(latte_rocksdb_t* rocksdb,
        char* name);

latte_error_t* latte_rocksdb_write_cf(latte_rocksdb_t* rocksdb, const char* cf_name, ...);

/**
 * 从指定列族读取 key 对应的 value。
 * 成功时 *out_value 为 RocksDB 分配的内存，调用方须用 rocksdb_free(*out_value) 释放；
 * key 不存在时 *out_value 为 NULL，*out_value_len 为 0，仍返回 &Ok。
 */
latte_error_t* latte_rocksdb_get_cf(latte_rocksdb_t* rocksdb, const char* cf_name,
    const char* key, size_t key_len, char** out_value, size_t* out_value_len);

latte_error_t* latte_rocksdb_flush(latte_rocksdb_t* rocksdb, const char* cf_name);

/**
 * Compact the given column family.
 * If start_key and limit_key are NULL (with lengths 0), compact the entire column family.
 * @param start_key  range start (NULL for full compact)
 * @param limit_key  range limit (NULL for full compact)
 */
latte_error_t* latte_rocksdb_compact_cf(latte_rocksdb_t* rocksdb, const char* cf_name,
    const char* start_key, size_t start_key_len,
    const char* limit_key, size_t limit_key_len);

#endif