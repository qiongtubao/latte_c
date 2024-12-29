#include "latte_rocksdb.h"
#include "zmalloc/zmalloc.h"

#define KB 1024
#define MB (1024*1024)
void init_default_rocksdb_db_opts(latte_rocksdb_t* rocks) {

    rocksdb_options_set_create_if_missing(rocks->db_opts, 1); //如果指定的数据库
    rocksdb_options_set_create_missing_column_families(rocks->db_opts, 1);
    rocksdb_options_optimize_for_point_lookup(rocks->db_opts, 1);

    rocksdb_options_set_min_write_buffer_number_to_merge(rocks->db_opts, 2);
    rocksdb_options_set_level0_file_num_compaction_trigger(rocks->db_opts, 2);
    rocksdb_options_set_max_bytes_for_level_base(rocks->db_opts, 256*MB);
    rocksdb_options_compaction_readahead_size(rocks->db_opts, 2*1024*1024); /* default 0 */

    // rocksdb_options_set_max_background_jobs(rocks->db_opts, server.rocksdb_max_background_jobs); /* default 4 */
    rocksdb_options_set_max_background_jobs(rocks->db_opts, 4); /* default 4 */
    // rocksdb_options_set_max_background_compactions(rocks->db_opts, server.rocksdb_max_background_compactions); /* default 4 */
    rocksdb_options_set_max_background_compactions(rocks->db_opts, 4);
    // rocksdb_options_set_max_background_flushes(rocks->db_opts, server.rocksdb_max_background_flushes); /* default -1 */
    rocksdb_options_set_max_background_flushes(rocks->db_opts, -1); /* default -1 */
    // rocksdb_options_set_max_subcompactions(rocks->db_opts, server.rocksdb_max_subcompactions); /* default 4 */
    rocksdb_options_set_max_subcompactions(rocks->db_opts, 4);
    // rocksdb_options_set_max_open_files(rocks->db_opts,server.rocksdb_max_open_files);
    rocksdb_options_set_max_open_files(rocks->db_opts, -1);
    // rocksdb_options_set_enable_pipelined_write(rocks->db_opts,server.rocksdb_enable_pipelined_write);
    rocksdb_options_set_enable_pipelined_write(rocks->db_opts, 0);

    rocksdb_options_set_max_manifest_file_size(rocks->db_opts, 64*MB);
    rocksdb_options_set_max_log_file_size(rocks->db_opts, 256*MB);
    rocksdb_options_set_keep_log_file_num(rocks->db_opts, 12);
}

void init_default_rocksdb_read_opts(latte_rocksdb_t* rocks) {
    rocksdb_readoptions_set_verify_checksums(rocks->ropts, 0);
    rocksdb_readoptions_set_fill_cache(rocks->ropts, 1);
}

void init_default_rocksdb_write_opts(latte_rocksdb_t* rocks) {
    rocksdb_writeoptions_disable_WAL(rocks->wopts, 1);
}

void init_default_rocksdb_flush_opts(latte_rocksdb_t* rocks) {
    rocksdb_flushoptions_set_wait(rocks->fopts, 1);
}

latte_rocksdb_t* latte_rocksdb_new() {
    latte_rocksdb_t* o = zmalloc(sizeof(latte_rocksdb_t));
    o->db = NULL;
    o->family_count = 0;
    o->family_info = NULL;
    o->db_opts = rocksdb_options_create();  //创建rocksdb
    init_default_rocksdb_db_opts(o);        
    o->wopts = rocksdb_writeoptions_create();
    init_default_rocksdb_write_opts(o);
    o->ropts = rocksdb_readoptions_create();
    init_default_rocksdb_read_opts(o);
    o->fopts = rocksdb_flushoptions_create();
    init_default_rocksdb_flush_opts(o);
    return o;
}

latte_error_t* latte_rocksdb_open(latte_rocksdb_t* rocksdb, char* dir, int count, latte_rocksdb_family_t** family_info) {
    char* errs[count];
    char* swap_cf_names[count];
    rocksdb_options_t* cf_opts[count];
    rocksdb_column_family_handle_t* cf_handles[count];
    for(int i = 0; i < count; i++) {
        swap_cf_names[i] = family_info[i]->cf_name;
        cf_opts[i] = family_info[i]->cf_opts;
        cf_handles[i] = NULL;
        errs[i] = NULL;
    }
    rocksdb->db = rocksdb_open_column_families(rocksdb->db_opts, dir, count,
            (const char *const *)swap_cf_names, (const rocksdb_options_t *const *)cf_opts,
            cf_handles, errs);
    for( int i = 0; i < count; i++) {
        if (errs[i] != NULL) {
            return error_new(CIOError, errs[i], NULL);
        }
        family_info[i]->cf_handle = cf_handles[i];
    }
    rocksdb->family_count = count;
    rocksdb->family_info = family_info;
    return &Ok;
}

int latte_rocksdb_read(latte_rocksdb_t* db, size_t count, latte_rocksdb_query_t** querys) {
    rocksdb_column_family_handle_t** cfs_list = zmalloc(count* sizeof(rocksdb_column_family_handle_t*));
    char** keys_list = zmalloc(count* sizeof(char*));
    size_t *keys_list_sizes = zmalloc(count*sizeof(size_t));
    for(size_t i = 0; i < count; i++) {
        cfs_list[i] = querys[i]->cf;
        keys_list[i] = querys[i]->key;
        keys_list_sizes[i] = sds_len(querys[i]->key);
    }
    char **errs = zmalloc(count*sizeof(char*));
    char **values_list = zmalloc(count*sizeof(char*));
    size_t *values_list_sizes = zmalloc(count*sizeof(size_t));
    rocksdb_multi_get_cf(db->db, db->ropts, (const rocksdb_column_family_handle_t *const *) cfs_list,
        count, (const char**)keys_list, (const size_t*)keys_list_sizes, 
        values_list, values_list_sizes, errs);
    int error_count = 0;
    for (size_t i = 0; i < count; i++) {
        if (errs[i] != NULL) {
            querys[i]->error = error_new(CIOError,"rockdb",sds_new(errs[i]));
            zlibc_free(errs[i]);
            error_count++;
        } else {
            querys[i]->value = sds_new_len(values_list[i], values_list_sizes[i]);
            zlibc_free(values_list[i]);
        }
    }
    zfree(cfs_list);
    zfree(keys_list);
    zfree(values_list);
    zfree(keys_list_sizes);
    zfree(values_list_sizes);
    zfree(errs);
    return error_count;
}

latte_error_t* latte_rocksdb_write(latte_rocksdb_t* db, size_t count, latte_rocksdb_put_t** puts) {
    char* err = NULL;
    rocksdb_writebatch_t* wb = rocksdb_writebatch_create();
    
    for(size_t i = 0; i < count; i++) {
        rocksdb_writebatch_put_cf(wb, puts[i]->cf, 
            puts[i]->key, sds_len(puts[i]->key),
            puts[i]->value, sds_len(puts[i]->value));
    }
    
    rocksdb_write(db->db, db->wopts, wb, &err);
    
    latte_error_t* error = &Ok;
    if (err != NULL) {
        error = error_new(CIOError, "rocksdb", sds_new(err));
        zlibc_free(err);
    }
    rocksdb_writebatch_destroy(wb);
    return error;
}


void init_default_rocksdb_family(latte_rocksdb_family_t* family) {
    rocksdb_options_set_compression(family->cf_opts, 1);//设置使用压缩算法 值1为snappy算法
    rocksdb_options_set_level0_slowdown_writes_trigger(family->cf_opts, 20);//当level-0文件数量达到20时 RocksDB 将开始减慢写入操作以避免过多的合并操作。这有助于防止写入放大问题。
    rocksdb_options_set_disable_auto_compactions(family->cf_opts, 0); //设置为0表示不禁用自动压缩
    rocksdb_options_set_enable_blob_files(family->cf_opts, 0); //设置为0表示不启动blob文件支持 blob文件用于存储大对象  
    rocksdb_options_set_enable_blob_gc(family->cf_opts, 0); //设置为0 表示不启动 blob文件的垃圾回收 如果未启动blob文件 设置无关紧要
    rocksdb_options_set_min_blob_size(family->cf_opts, 4096); //设置最小blob大小为4KB  这个设置决定了哪些值会被存储在blob文件中
    rocksdb_options_set_blob_file_size(family->cf_opts, 256*1024*1024);//设置blob文件的最大大小为256MB
    rocksdb_options_set_blob_gc_age_cutoff(family->cf_opts, (double)5/ 100);//设置blob文件垃圾回收的年龄截止为5% 
    rocksdb_options_set_blob_gc_force_threshold(family->cf_opts, (double)90 / 100);//设置强制进行blob文件垃圾回收的阈值为90%
    rocksdb_options_set_max_write_buffer_number(family->cf_opts, 4);//设置最大写入缓冲区的数量为4 当有多个写入缓冲区时，可以提高写入吞吐量 并且允许更灵活地管理内存使用。
    rocksdb_options_set_target_file_size_base(family->cf_opts, 32*1024*1024);//设置基础级别的目标文件大小为32MB。这影响了level-1和最高层的数据文件大小
    rocksdb_options_set_write_buffer_size(family->cf_opts,64*1024*1024);//设置单个写入缓冲区的大小为64MB 较大的写入缓冲区可以减少磁盘I/O操作 但会占用更多内存
    rocksdb_options_set_max_bytes_for_level_base(family->cf_opts,512*1024*1024);//设置基础级别的最大字节数为512MB 这是level-1的总大小
    rocksdb_options_set_max_bytes_for_level_multiplier(family->cf_opts, 10);//设置每个级别之间的大小倍增因子为10，这意味着level-2的总大小将是level-1的10倍
    rocksdb_options_set_level_compaction_dynamic_level_bytes(family->cf_opts, 0);//设置是否根据数据分布动态调整各级别的字节数，设置为0 表示禁用动态调整
    
    rocksdb_options_add_compact_on_deletion_collector_factory(family, 100000, 80000, 0.95);

    rocksdb_block_based_table_options_t *  block_opts = rocksdb_block_based_options_create();
    rocksdb_block_based_options_set_block_size(block_opts, 8192);
    rocksdb_block_based_options_set_cache_index_and_filter_blocks(block_opts, 0);
    rocksdb_block_based_options_set_filter_policy(block_opts, rocksdb_filterpolicy_create_bloom(10));
    //创建并配置LRU缓存
    rocksdb_cache_t *data_cache = rocksdb_cache_create_lru(8*1024*1024);
    rocksdb_block_based_options_set_block_cache(block_opts, data_cache);    
    rocksdb_cache_destroy(data_cache);

    //设置块级别表工厂，并将块选项应用到列族选项中
    rocksdb_options_set_block_based_table_factory(family, block_opts);
    //销毁块选项
    rocksdb_block_based_options_destroy(block_opts);

    //设置合并过滤器工厂 
    // rocksdb_options_set_compaction_filter_factory(family, createDataCfCompactionFilterFactory());

}

latte_rocksdb_family_t* latte_rocksdb_family_create(latte_rocksdb_t* db, sds cf_name) {
    latte_rocksdb_family_t* family = zmalloc(sizeof(latte_rocksdb_family_t));
    family->cf_name = cf_name;
    family->cf_opts = rocksdb_options_create_copy(db->db_opts);
    init_default_rocksdb_family(family);
    family->cf_handle = NULL;
    return family;
}

latte_rocksdb_put_t* latte_rocksdb_put_new(rocksdb_column_family_handle_t* cf, sds key, sds value) {
    latte_rocksdb_put_t* put = zmalloc(sizeof(latte_rocksdb_put_t));
    put->cf = cf;
    put->key = key;
    put->value = value;
    return put;
}

void latte_rocksdb_put_delete(latte_rocksdb_put_t* put) {
    sds_delete(put->key);
    sds_delete(put->value);
    zfree(put);
}



latte_rocksdb_query_t* latte_rocksdb_query_new(rocksdb_column_family_handle_t* cf, sds key) {
    latte_rocksdb_query_t* query = zmalloc(sizeof(latte_rocksdb_query_t));
    query->cf = cf;
    query->key = key;
    query->error = NULL;
    query->value = NULL;
    return query;
}

void latte_rocksdb_query_delete(latte_rocksdb_query_t* query) {
    sds_delete(query->key);
    sds_delete(query->value);
    if(query->error != NULL && !error_is_ok(query->error)) {
        error_delete(query->error);
    }
    zfree(query);
}


latte_error_t* latte_rocksdb_flush(latte_rocksdb_t* db) {
    char* err = NULL;
    rocksdb_flush(db->db, db->fopts, &err);
    latte_error_t* error = &Ok;
    if (err != NULL) {
        error = error_new(CIOError, "rocksdb", sds_new(err));
        zlibc_free(err);
    }
    return error;
}