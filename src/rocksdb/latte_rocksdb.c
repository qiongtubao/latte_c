#include "latte_rocksdb.h"

#include "error/error.h"
#include "dict/dict.h"
#include "dict/dict_plugins.h"
#include "iterator/iterator.h"

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
    meta->cf_opts = rocksdb_options_create_copy(rocksdb->db_opts);
    meta->handle = NULL;
    dict_add(rocksdb->column_families_metas, meta_name, meta);
    
    return meta;
}



latte_error_t* latte_rocksdb_open(latte_rocksdb_t* rocksdb) {

    size_t size = dict_size(rocksdb->column_families_metas);
    const char* cf_name[size];
    const rocksdb_options_t* cf_opts[size];
    rocksdb_column_family_handle_t* cf_handles[size];
    char* errs[size];

    latte_iterator_t* iterator = dict_get_latte_iterator(rocksdb->column_families_metas);
    size_t i = 0;

    while(latte_iterator_has_next(iterator)) {
        latte_rocksdb_column_family_meta_t* meta = latte_iterator_next(iterator);
        cf_name[i] = meta->name;
        cf_opts[i] = meta->cf_opts;
        cf_handles[i] = NULL;
        printf("cf_name: %s %p %p\n", meta->name,  meta->cf_opts, cf_opts[i] );
        i++;
    }
    latte_iterator_delete(iterator);
    rocksdb->db = rocksdb_open_column_families(rocksdb->db_opts, (const char*)rocksdb->db_path,
        (int)size,
        cf_name, cf_opts, cf_handles, errs);
    printf("latte_rocksdb_open\n");

    iterator = dict_get_latte_iterator(rocksdb->column_families_metas);
    i = 0;
    while(latte_iterator_has_next(iterator)) {
        latte_rocksdb_column_family_meta_t* meta = latte_iterator_next(iterator);
        if (cf_name[i] == meta->name) {
            if (errs[i] != NULL) {
                return error_new(-1, "rocksdb open column family error: %s", errs[i]);
            }
            meta->handle = cf_handles[i];
        } else {
            return error_new(-1, "rocksdb meta not match", "");
        }

        i++;
    }
    latte_iterator_delete(iterator);

    return &Ok;
}

void latte_rocksdb_close(latte_rocksdb_t* db) {
    
    zfree(db);
}