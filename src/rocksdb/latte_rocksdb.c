#include "latte_rocksdb.h"
#include <stdarg.h>

#include "error/error.h"
#include "dict/dict.h"
#include "dict/dict_plugins.h"
#include "iterator/iterator.h"
#include "utils/utils.h"

struct dict_func_t latte_rocksdb_meta_dict_type = {
    dict_char_hash,                    /* hash function */
    NULL,                           /* key dup */
    NULL,                           /* val dup */
    dict_char_key_compare,              /* key compare */
    NULL,              /* key destructor */
    NULL,                           /* val destructor */
    NULL                            /* allow to expand */
};

latte_rocksdb_t* latte_rocksdb_create(sds dir_path) {
    latte_rocksdb_t* rocksdb = zmalloc(sizeof(latte_rocksdb_t));
    rocksdb->db = NULL;
    rocksdb->db_path = dir_path;
    rocksdb->db_opts = rocksdb_options_create();
    rocksdb->column_families_metas = dict_new(&latte_rocksdb_meta_dict_type);
    rocksdb->read_opts = rocksdb_readoptions_create();
    rocksdb->write_opts = rocksdb_writeoptions_create();
    rocksdb->compact_opts = rocksdb_compactoptions_create();
    rocksdb->flush_opts = rocksdb_flushoptions_create();
    return rocksdb;
}

latte_rocksdb_column_family_meta_t* latte_rocksdb_add_column_family(latte_rocksdb_t* rocksdb,
        char* name) {
    sds meta_name = sds_new(name);

    latte_rocksdb_column_family_meta_t* meta = zmalloc(sizeof(latte_rocksdb_column_family_meta_t));
    meta->name = meta_name;

    // 创建一个新的 options 而不是拷贝 DB options
    // 因为 memtable_factory 默认是 SkipListFactory，如果拷贝 DB options 可能没有正确的默认值
    meta->cf_opts = rocksdb_options_create();
    

    meta->handle = NULL;
    dict_add(rocksdb->column_families_metas, meta_name, meta);

    return meta;
}



latte_error_t* latte_rocksdb_open(latte_rocksdb_t* rocksdb) {

    int size = dict_size(rocksdb->column_families_metas);
    const char* cf_name[size];
    const rocksdb_options_t* cf_opts[size];
    const rocksdb_column_family_handle_t* cf_handles[size];
    char* errs = NULL;

    latte_iterator_t* it = dict_get_latte_iterator(rocksdb->column_families_metas);
    int i = 0;

    while(latte_iterator_has_next(it)) {
        latte_pair_t* pair = latte_iterator_next(it);
        latte_rocksdb_column_family_meta_t*  meta = latte_pair_value(pair);

        cf_name[i] = meta->name;
        cf_opts[i] = meta->cf_opts;
        cf_handles[i] = NULL;
        i++;
    }
    latte_assert_with_info(i == size, "column family meta size not match");
    latte_iterator_delete(it);
    
    rocksdb->db = rocksdb_open_column_families(rocksdb->db_opts, rocksdb->db_path,
        size,
        cf_name, cf_opts, cf_handles, &errs);
  


    latte_error_t* err = NULL;
    if (errs != NULL || rocksdb-> db == NULL) {
        latte_error_t* err = error_new(-1, "rocksdb_open_column_families fail: %s", errs);
        rocksdb_free(errs);
        return err;
    }
    for(i = 0; i < size; i++) {
        if (cf_handles[i] == NULL) continue;
        sds name = cf_name[i];
        dict_entry_t* entry = dict_find(rocksdb->column_families_metas, name);
        latte_assert_with_info(entry != NULL, "column family meta not found");
        latte_rocksdb_column_family_meta_t* meta = dict_get_entry_val(entry);
        meta->handle = cf_handles[i];
    } 
    return &Ok;
}

latte_error_t* latte_rocksdb_write_cf(latte_rocksdb_t* rocksdb, const char* cf_name, ...) {
    dict_entry_t* entry = dict_find(rocksdb->column_families_metas, cf_name);
    if (entry == NULL) {
        return error_new(-1, "column family not found: %s", cf_name);
    }

    latte_rocksdb_column_family_meta_t* meta = dict_get_entry_val(entry);
    rocksdb_writebatch_t* batch = rocksdb_writebatch_create();
    char* errs = NULL;

    va_list args;
    va_start(args, cf_name);

    while (1) {
        const char* key = va_arg(args, const char*);
        if (key == NULL) break;

        size_t key_len = va_arg(args, size_t);
        const char* value = va_arg(args, const char*);
        size_t value_len = va_arg(args, size_t);

        rocksdb_writebatch_put_cf(batch, meta->handle, key, key_len, value, value_len);
    }

    va_end(args);

    rocksdb_write(rocksdb->db, rocksdb->write_opts, batch, &errs);
    rocksdb_writebatch_destroy(batch);

    if (errs != NULL) {
        latte_error_t* err = error_new(-1, "rocksdb_write fail: %s", errs);
        rocksdb_free(errs);
        return err;
    }

    return &Ok;
}

latte_error_t* latte_rocksdb_get_cf(latte_rocksdb_t* rocksdb, const char* cf_name,
    const char* key, size_t key_len, char** out_value, size_t* out_value_len) {
    if (out_value) *out_value = NULL;
    if (out_value_len) *out_value_len = 0;

    dict_entry_t* entry = dict_find(rocksdb->column_families_metas, cf_name);
    if (entry == NULL) {
        return error_new(-1, "column family not found: %s", cf_name);
    }

    latte_rocksdb_column_family_meta_t* meta = dict_get_entry_val(entry);
    char* errs = NULL;
    size_t vlen = 0;
    char* val = rocksdb_get_cf(rocksdb->db, rocksdb->read_opts, meta->handle,
        key, key_len, &vlen, &errs);

    if (errs != NULL) {
        latte_error_t* err = error_new(-1, "rocksdb_get_cf fail: %s", errs);
        rocksdb_free(errs);
        return err;
    }
    if (out_value) *out_value = val;
    if (out_value_len) *out_value_len = vlen;
    return &Ok;
}

latte_error_t* latte_rocksdb_flush(latte_rocksdb_t* rocksdb, const char* cf_name) {
    // 查找指定的 column family
    dict_entry_t* entry = dict_find(rocksdb->column_families_metas, cf_name);
    if (entry == NULL) {
        return error_new(-1, "column family not found: %s", cf_name);
    }

    latte_rocksdb_column_family_meta_t* meta = dict_get_entry_val(entry);
    char* errs = NULL;

    // // 创建 flush 选项
    // rocksdb_flushoptions_t* flush_opts = rocksdb_flushoptions_create();

    // // 设置 wait 为 1，表示等待 flush 完成
    // rocksdb_flushoptions_set_wait(flush_opts, 1);

    // 执行 flush 操作 - 使用 rocksdb_flush_cf 来指定 column family
    rocksdb_flush_cf(rocksdb->db, rocksdb->flush_opts, meta->handle, &errs);

    // 销毁 flush 选项
    // rocksdb_flushoptions_destroy(flush_opts);

    // 检查错误
    if (errs != NULL) {
        latte_error_t* err = error_new(-1, "rocksdb_flush fail: %s", errs);
        rocksdb_free(errs);
        return err;
    }

    return &Ok;
}

/*
 * latte_rocksdb_compact_cf：对指定列族执行压缩（Compaction）
 *
 * 作用简述：
 * - 合并/重写该列族内的 SST 文件，减少文件数量和层级，提升后续读性能
 * - 回收已删除或过期数据占用的磁盘空间
 * - 可全量压缩或仅压缩 [start_key, limit_key) 范围内的数据
 *
 * 参数说明：
 * - rocksdb：已打开的 RocksDB 实例
 * - cf_name：列族名称（如 "default"、"meta"）
 * - start_key / start_key_len：压缩范围的起始 key；传 NULL 和 0 表示从最小 key 开始
 * - limit_key / limit_key_len：压缩范围的结束 key（不包含）；传 NULL 和 0 表示到最大 key
 *
 * 使用注意：
 * - 全量压缩：start_key 与 limit_key 均传 NULL，长度传 0
 * - 范围压缩：传入有效的 key 指针及对应长度，压缩 [start_key, limit_key) 左闭右开区间
 * - 本函数为同步调用，会阻塞直到该次压缩完成
 *
 * 返回值：成功返回 &Ok；列族不存在时返回错误
 */
latte_error_t* latte_rocksdb_compact_cf(latte_rocksdb_t* rocksdb, const char* cf_name,
    const char* start_key, size_t start_key_len,
    const char* limit_key, size_t limit_key_len) {
    /* 根据列族名查找列族元数据 */
    dict_entry_t* entry = dict_find(rocksdb->column_families_metas, cf_name);
    if (entry == NULL) {
        return error_new(-1, "column family not found: %s", cf_name);
    }

    latte_rocksdb_column_family_meta_t* meta = dict_get_entry_val(entry);
    /* 调用 RocksDB C API：对指定 handle 的列族做范围/全量压缩 */
    rocksdb_compact_range_cf(rocksdb->db, meta->handle,
        start_key, start_key_len, limit_key, limit_key_len);
    return &Ok;
}

/*
 * latte_rocksdb_close：关闭 RocksDB 并释放所有相关资源
 *
 * 释放顺序（避免悬空指针与重复释放）：
 * 1. 关闭数据库实例（rocksdb_close），停止后台任务并落盘
 * 2. 遍历列族元数据：销毁列族 handle、列族 options，释放列族名 sds 及 meta 结构体
 * 3. 销毁并释放 column_families_metas 字典
 * 4. 销毁 DB 级 options、read_opts、write_opts
 * 5. 释放 db_path（sds）及 latte_rocksdb_t 自身
 *
 * 使用注意：
 * - 传入 NULL 时直接返回，不做任何操作
 * - 若 db 未曾 open，db->db 为 NULL，仅释放 create 阶段分配的资源
 * - 调用后 db 指针不可再使用（不可重复 close）
 */
void latte_rocksdb_close(latte_rocksdb_t* db) {
    if (db == NULL) {
        return;
    }

    /* 1a. 先销毁所有列族 handle（RocksDB 要求先 close 再 destroy handle，此处保持先 destroy 再 close 以规避部分环境下的崩溃） */
    if (db->column_families_metas != NULL && db->db != NULL) {
        latte_iterator_t* it = dict_get_latte_iterator(db->column_families_metas);
        while (latte_iterator_has_next(it)) {
            latte_pair_t* pair = latte_iterator_next(it);
            latte_rocksdb_column_family_meta_t* meta =
                (latte_rocksdb_column_family_meta_t*)latte_pair_value(pair);
            if (meta->handle != NULL) {
                rocksdb_column_family_handle_destroy(meta->handle);
                meta->handle = NULL;
            }
        }
        latte_iterator_delete(it);
    }

    /* 1b. 关闭 RocksDB 实例 */
    if (db->db != NULL) {
        rocksdb_close(db->db);
        db->db = NULL;
    }

    /* 2. 释放每个列族的 options/name，收集 meta 后统一 zfree，再 dict_delete */
    if (db->column_families_metas != NULL) {
        size_t n = (size_t)dict_size(db->column_families_metas);
        if (n > 0) {
            latte_rocksdb_column_family_meta_t** metas =
                (latte_rocksdb_column_family_meta_t**)zmalloc(n * sizeof(latte_rocksdb_column_family_meta_t*));
            latte_iterator_t* it = dict_get_latte_iterator(db->column_families_metas);
            size_t i = 0;
            while (latte_iterator_has_next(it)) {
                latte_pair_t* pair = latte_iterator_next(it);
                latte_rocksdb_column_family_meta_t* meta =
                    (latte_rocksdb_column_family_meta_t*)latte_pair_value(pair);
                if (meta->cf_opts != NULL) {
                    rocksdb_options_destroy(meta->cf_opts);
                    meta->cf_opts = NULL;
                }
                if (meta->name != NULL) {
                    sds_delete(meta->name);
                    meta->name = NULL;
                }
                metas[i++] = meta;
            }
            latte_iterator_delete(it);
            for (i = 0; i < n; i++) {
                zfree(metas[i]);
            }
            zfree(metas);
        }
        dict_delete(db->column_families_metas);
        db->column_families_metas = NULL;
    }

    /* 3. 销毁 DB 级 options 与读写选项 */
    if (db->db_opts != NULL) {
        rocksdb_options_destroy(db->db_opts);
        db->db_opts = NULL;
    }
    if (db->read_opts != NULL) {
        rocksdb_readoptions_destroy(db->read_opts);
        db->read_opts = NULL;
    }
    if (db->write_opts != NULL) {
        rocksdb_writeoptions_destroy(db->write_opts);
        db->write_opts = NULL;
    }
    if (db->compact_opts != NULL) {
        rocksdb_compactoptions_destroy(db->compact_opts);
        db->compact_opts = NULL;
    }
    if (db->flush_opts != NULL) {
        rocksdb_flushoptions_destroy(db->flush_opts);
        db->flush_opts = NULL;
    }

    /* 4. 释放 db_path 及自身结构体 */
    if (db->db_path != NULL) {
        sds_delete(db->db_path);
        db->db_path = NULL;
    }
    zfree(db);
}