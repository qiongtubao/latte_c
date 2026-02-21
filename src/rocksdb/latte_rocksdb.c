#include "latte_rocksdb.h"

#include "error/error.h"
#include "dict/dict.h"
#include "dict/dict_plugins.h"
#include "iterator/iterator.h"
#include "utils/utils.h"

struct dict_func_t latte_rocksdb_meta_dict_type = {
    dict_sds_hash,                    /* hash function */
    NULL,                           /* key dup */
    NULL,                           /* val dup */
    dict_sds_key_compare,              /* key compare */
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

void latte_rocksdb_close(latte_rocksdb_t* db) {
    
    zfree(db);
}