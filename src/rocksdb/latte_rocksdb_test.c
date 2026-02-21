#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <stdio.h>
#include "./latte_rocksdb.h" 
#include "rocksdb/c.h"

latte_rocksdb_t* test_create_rocksdb(char* db_path) {
    latte_rocksdb_t* rocksdb = latte_rocksdb_create(sds_new(db_path));

    // 设置数据库不存在时自动创建
    LATTE_SET_DB_OPTION(rocksdb, set_create_if_missing, 1);

    // // 设置Column Family不存在时自动创建
    LATTE_SET_DB_OPTION(rocksdb, set_create_missing_column_families, 1);

    // // 优化点查询性能
    LATTE_SET_DB_OPTION(rocksdb, optimize_for_point_lookup, 1);

    // 设置最小写缓冲区合并数量
    LATTE_SET_DB_OPTION(rocksdb, set_min_write_buffer_number_to_merge, 2);

    // 设置Level 0的最大字节数
    LATTE_SET_DB_OPTION(rocksdb, set_max_bytes_for_level_base, 256*1024*1024);

    // 设置压缩预读大小
    LATTE_SET_DB_OPTION(rocksdb, compaction_readahead_size, 2*1024*1024);

    // 设置最大后台任务数
    LATTE_SET_DB_OPTION(rocksdb, set_max_background_jobs, 2);

    // 设置最大后台压缩任务数
    LATTE_SET_DB_OPTION(rocksdb, set_max_background_compactions, 2);


    // 设置最大后台flush任务数
    LATTE_SET_DB_OPTION(rocksdb, set_max_background_flushes, -1);

    // 设置最大子压缩任务数
    LATTE_SET_DB_OPTION(rocksdb, set_max_subcompactions, 1);

    // 设置最大打开文件数
    LATTE_SET_DB_OPTION(rocksdb, set_max_open_files, -1);

    // 启用管道写
    LATTE_SET_DB_OPTION(rocksdb, set_enable_pipelined_write, 0);

    // 设置最大Manifest文件大小
    LATTE_SET_DB_OPTION(rocksdb, set_max_manifest_file_size, 64*1024*1024);

    // 设置最大WAL文件大小
    LATTE_SET_DB_OPTION(rocksdb, set_max_log_file_size, 256*1024*1024);

    // 设置保留的WAL文件数量
    LATTE_SET_DB_OPTION(rocksdb, set_keep_log_file_num, 12);

    // 创建速率限制器
    rocksdb_ratelimiter_t *ratelimiter = rocksdb_ratelimiter_create(
            512 * 1024 * 1024,
            100*1000,
            10);
    // 设置速率限制器
    LATTE_SET_DB_OPTION(rocksdb, set_ratelimiter, ratelimiter);

    // 销毁速率限制器
    rocksdb_ratelimiter_destroy(ratelimiter);

    // 设置每sync的字节数
    LATTE_SET_DB_OPTION(rocksdb, set_bytes_per_sync, 1*1024*1024);
    
    // 设置读配置
    // 设置是否验证校验和
    LATTE_SET_DB_READ_OPTION(rocksdb, set_verify_checksums, 0);
    // 设置是否填充块缓存
    LATTE_SET_DB_READ_OPTION(rocksdb, set_fill_cache, 1);
    // 设置是否启用异步IO
    LATTE_SET_DB_READ_OPTION(rocksdb, set_async_io, 0);


    //设置写配置
    // 禁用WAL
    LATTE_SET_DB_WRITE_OPTION(rocksdb, disable_WAL, 1);

    latte_rocksdb_column_family_meta_t* default_meta = latte_rocksdb_add_column_family(rocksdb, "default");
    int level_values[7] = {
        rocksdb_no_compression,
        rocksdb_no_compression,
        rocksdb_snappy_compression,
        rocksdb_snappy_compression,
        rocksdb_snappy_compression,
        rocksdb_snappy_compression,
        rocksdb_snappy_compression,
    };
    // // 按Level设置压缩算法
    LATTE_ROCKSDB_SET_CF_OPTION(default_meta, set_compression_per_level, level_values, 7);
    // 设置Level 0写操作减速触发阈值
    LATTE_ROCKSDB_SET_CF_OPTION(default_meta, set_level0_slowdown_writes_trigger, 20);
    // 设置是否禁用自动压缩
    LATTE_ROCKSDB_SET_CF_OPTION(default_meta, set_disable_auto_compactions, 0);
    // 设置是否启用Blob文件
    LATTE_ROCKSDB_SET_CF_OPTION(default_meta, set_enable_blob_files, 0);
    // 设置是否启用Blob垃圾回收
    LATTE_ROCKSDB_SET_CF_OPTION(default_meta, set_enable_blob_gc, 1);
    // 设置最小Blob大小
    LATTE_ROCKSDB_SET_CF_OPTION(default_meta, set_min_blob_size, 4096);
    // 设置Blob文件大小
    LATTE_ROCKSDB_SET_CF_OPTION(default_meta, set_blob_file_size, 256 * 1024 * 1024);
    // 设置Blob垃圾回收年龄截止阈值
    LATTE_ROCKSDB_SET_CF_OPTION(default_meta, set_blob_gc_age_cutoff,  5/ 100);
    // 设置Blob垃圾回收强制阈值
    LATTE_ROCKSDB_SET_CF_OPTION(default_meta, set_blob_gc_force_threshold, 50 / 100);
    // 设置最大写缓冲区数量
    LATTE_ROCKSDB_SET_CF_OPTION(default_meta, set_max_write_buffer_number, 4);
    // 设置目标文件大小基数
    LATTE_ROCKSDB_SET_CF_OPTION(default_meta, set_target_file_size_base, 32 * 1024 * 1024);
    // 设置写缓冲区大小
    LATTE_ROCKSDB_SET_CF_OPTION(default_meta, set_write_buffer_size, 64 * 1024 * 1024);
    // 设置Level最大字节数基数
    LATTE_ROCKSDB_SET_CF_OPTION(default_meta, set_max_bytes_for_level_base, 512 * 1024 * 1024);
    // 设置Level最大字节数倍数
    LATTE_ROCKSDB_SET_CF_OPTION(default_meta, set_max_bytes_for_level_multiplier, 8);
    // 设置动态Level字节大小压缩
    LATTE_ROCKSDB_SET_CF_OPTION(default_meta, set_level_compaction_dynamic_level_bytes, 1);
    // 设置Level 0文件数量触发压缩阈值
    LATTE_ROCKSDB_SET_CF_OPTION(default_meta, set_level0_file_num_compaction_trigger, 4);
    // 添加基于删除比率的压缩触发器
    LATTE_ROCKSDB_SET_CF_OPTION(default_meta, add_compact_on_deletion_collector_factory_del_ratio, 
        100000,
        80000,
        (double)95/100);

    //创建块表选项对象
    rocksdb_block_based_table_options_t* block_opts = rocksdb_block_based_options_create();
    // 设置块大小
    rocksdb_block_based_options_set_block_size(block_opts, 8192);
    // 设置缓存索引和过滤器块
    rocksdb_block_based_options_set_cache_index_and_filter_blocks(block_opts, 0);
    // 设置过滤器策略（布隆过滤器）
    rocksdb_block_based_options_set_filter_policy(block_opts, rocksdb_filterpolicy_create_bloom(10));
    // 创建LRU块缓存
    rocksdb_cache_t *data_cache = rocksdb_cache_create_lru(8*1024*1024);
    // 设置块缓存
    rocksdb_block_based_options_set_block_cache(block_opts, data_cache);
    // 销毁块缓存
    rocksdb_cache_destroy(data_cache);
    // 设置块表工厂
    LATTE_ROCKSDB_SET_CF_OPTION(default_meta, set_block_based_table_factory, block_opts);
    // 销毁块表选项对象
    rocksdb_block_based_options_destroy(block_opts);
   

    latte_rocksdb_column_family_meta_t* meta_meta = latte_rocksdb_add_column_family(rocksdb, "meta");
    // // 按Level设置压缩算法
    LATTE_ROCKSDB_SET_CF_OPTION(meta_meta, set_compression_per_level, level_values, 7);

    // 设置Level 0写操作减速触发阈值 
    LATTE_ROCKSDB_SET_CF_OPTION(meta_meta, set_level0_slowdown_writes_trigger, 20);
    // 设置是否禁用自动压缩
    LATTE_ROCKSDB_SET_CF_OPTION(meta_meta, set_disable_auto_compactions, 0);

    // 设置是否启用Blob文件
    LATTE_ROCKSDB_SET_CF_OPTION(meta_meta, set_enable_blob_files, 0);

    // 设置是否启用Blob垃圾回收
    LATTE_ROCKSDB_SET_CF_OPTION(meta_meta, set_enable_blob_gc, 1);

    // 设置最小Blob大小
    LATTE_ROCKSDB_SET_CF_OPTION(meta_meta, set_min_blob_size, 4096);

    // 设置Blob文件大小
    LATTE_ROCKSDB_SET_CF_OPTION(meta_meta, set_blob_file_size, 256 * 1024 * 1024);

    // 设置Blob垃圾回收年龄截止阈值
    LATTE_ROCKSDB_SET_CF_OPTION(meta_meta, set_blob_gc_age_cutoff, (double)5 / 100);

    // 设置Blob垃圾回收强制阈值
    LATTE_ROCKSDB_SET_CF_OPTION(meta_meta, set_blob_gc_force_threshold, (double)90 / 100);

    // 设置最大写缓冲区数量
    LATTE_ROCKSDB_SET_CF_OPTION(meta_meta, set_max_write_buffer_number, 3);

    // 设置目标文件大小基数
    LATTE_ROCKSDB_SET_CF_OPTION(meta_meta, set_target_file_size_base, 32 * 1024 * 1024);

    // 设置写缓冲区大小
    LATTE_ROCKSDB_SET_CF_OPTION(meta_meta, set_write_buffer_size, 64 * 1024 * 1024);

    // 设置Level最大字节数基数
    LATTE_ROCKSDB_SET_CF_OPTION(meta_meta, set_max_bytes_for_level_base, 256 * 1024 * 1024);

    // 设置Level最大字节数倍数
    LATTE_ROCKSDB_SET_CF_OPTION(meta_meta, set_max_bytes_for_level_multiplier, 10);

    // 设置动态Level字节大小压缩
    LATTE_ROCKSDB_SET_CF_OPTION(meta_meta, set_level_compaction_dynamic_level_bytes, 0);

    // 设置Level 0文件数量触发压缩阈值
    LATTE_ROCKSDB_SET_CF_OPTION(meta_meta, set_level0_file_num_compaction_trigger, 4);

    // 添加基于删除比率的压缩触发器
    LATTE_ROCKSDB_SET_CF_OPTION(meta_meta, add_compact_on_deletion_collector_factory_del_ratio, 
        100000,
        80000,
        (double)95/100);
        
    

    // 创建块表选项对象
    block_opts = rocksdb_block_based_options_create();
    // 设置块大小
    rocksdb_block_based_options_set_block_size(block_opts, 8192);
    // 设置缓存索引和过滤器块
    rocksdb_block_based_options_set_cache_index_and_filter_blocks(block_opts, 0);
    // 设置过滤器策略（布隆过滤器）
    rocksdb_block_based_options_set_filter_policy(block_opts, rocksdb_filterpolicy_create_bloom(10));
    // 创建LRU块缓存
    rocksdb_cache_t *meta_cache = rocksdb_cache_create_lru(512*1024*1024);
    // 设置块缓存
    rocksdb_block_based_options_set_block_cache(block_opts, meta_cache);
    // 销毁块缓存
    rocksdb_cache_destroy(meta_cache);
    // 设置块表工厂
    LATTE_ROCKSDB_SET_CF_OPTION(meta_meta, set_block_based_table_factory, block_opts);

    // 销毁块表选项对象
    rocksdb_block_based_options_destroy(block_opts);

    // 设置压缩过滤器工厂（META_CF不使用过滤器，传NULL）
    LATTE_ROCKSDB_SET_CF_OPTION(meta_meta, set_compaction_filter_factory, NULL);

 
    return rocksdb;
}

int test_rockdb_write() {
    latte_rocksdb_t* rocksdb = test_create_rocksdb("./write_test/");

    latte_error_t* error = latte_rocksdb_open(rocksdb);

    assert(error_is_ok(error));

    error = latte_rocksdb_write_cf(rocksdb, "default", "key1", "value", NULL);
    assert(error_is_ok(error));
    return 1;
}


int test_rocksdb_api_test() {
    // 1. 创建数据库选项
  rocksdb_options_t* db_options = rocksdb_options_create();
  rocksdb_options_set_create_if_missing(db_options, 1);

  // 2. 创建列族选项
  rocksdb_options_t* cf_options_default = rocksdb_options_create();
  rocksdb_options_t* cf_options_user = rocksdb_options_create();
  rocksdb_options_t* cf_options_order = rocksdb_options_create();

  // 3. 配置列族选项
  rocksdb_options_set_write_buffer_size(cf_options_user, 64 * 1024 * 1024);
  rocksdb_options_set_write_buffer_size(cf_options_order, 32 * 1024 * 1024);

  // 4. 准备列族名称和选项数组
  const char* cf_names[] = {"default", "user", "order"};
  rocksdb_options_t* cf_opts[] = {cf_options_default, cf_options_user, cf_options_order};
  int num_cfs = 3;

  // 5. 打开数据库
  rocksdb_column_family_handle_t* cf_handles[3];
  char* err = NULL;
  rocksdb_t* db = rocksdb_open_column_families(
      db_options, "./test", num_cfs,
      cf_names, cf_opts, cf_handles, &err);


  // 6. 检查错误
  if (err != NULL || db == NULL) {
      fprintf(stderr, "Failed to open database: %s\n", err);
      rocksdb_free(err);
      return -1;
  }

  // 7. 使用数据库
  rocksdb_put_cf(db, rocksdb_writeoptions_create(), cf_handles[1],
                 "user1", 5, "John Doe", 8, &err);

  // 8. 关闭数据库
  rocksdb_close(db);

  // 9. 释放资源
  for (int i = 0; i < num_cfs; i++) {
      rocksdb_column_family_handle_destroy(cf_handles[i]);
  }
  rocksdb_options_destroy(db_options);
  rocksdb_options_destroy(cf_options_default);
  rocksdb_options_destroy(cf_options_user);
  rocksdb_options_destroy(cf_options_order);
    return 1;
}

int test_api(void) {
    printf("test_api start\n");
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        printf("About to call test_rockdb_write\n");
        test_cond("rocksdb write function",
            test_rockdb_write() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}