#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "./latte_rocksdb.h"
#include "rocksdb/c.h"
#include "error/error.h"

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
    printf("write tid: %lld\n", pthread_self());

    // 测试1: 单次批量写入多组数据
    error = latte_rocksdb_write_cf(rocksdb, "default",
        "key1", 4, "value1", 6,
        "key2", 4, "value2", 6,
        "key3", 4, "value3", 6,
        NULL);
    assert(error_is_ok(error));

    // 测试2: 写入大量数据触发 memtable 切换
    // 设置较小的 write_buffer_size 以更容易触发 flush
    int total_keys = 10;
    for (int i = 0; i < total_keys; i++) {
        char key[32];
        char value[128];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d_with_longer_data_for_test", i);

        error = latte_rocksdb_write_cf(rocksdb, "default",
            key, strlen(key), value, strlen(value), NULL);
        assert(error_is_ok(error));
    }

    printf("Successfully wrote %d keys\n", total_keys);
    return 1;
}

/**
 * 单测：读（get_cf）
 * 写入若干 key/value，用 get_cf 读回并校验；再读不存在的 key 校验返回 NULL。
 */
int test_rocksdb_read() {
    printf("=== Testing RocksDB Read (get_cf) ===\n");

    latte_rocksdb_t* rocksdb = test_create_rocksdb("./read_test/");
    latte_error_t* err = latte_rocksdb_open(rocksdb);
    assert(error_is_ok(err));

    /* 写入 */
    err = latte_rocksdb_write_cf(rocksdb, "default",
        "rk1", 3, "rv1", 4,
        "rk2", 3, "rv2", 4,
        "rk3", 3, "rv3", 4,
        NULL);
    assert(error_is_ok(err));

    /* 读回并校验 */
    char* val = NULL;
    size_t vlen = 0;
    err = latte_rocksdb_get_cf(rocksdb, "default", "rk1", 3, &val, &vlen);
    assert(error_is_ok(err));
    assert(val != NULL && vlen == 4 && memcmp(val, "rv1", 4) == 0);
    rocksdb_free(val);
    val = NULL;

    err = latte_rocksdb_get_cf(rocksdb, "default", "rk2", 3, &val, &vlen);
    assert(error_is_ok(err));
    assert(val != NULL && vlen == 4 && memcmp(val, "rv2", 4) == 0);
    rocksdb_free(val);
    val = NULL;

    err = latte_rocksdb_get_cf(rocksdb, "default", "rk3", 3, &val, &vlen);
    assert(error_is_ok(err));
    assert(val != NULL && vlen == 4 && memcmp(val, "rv3", 4) == 0);
    rocksdb_free(val);
    val = NULL;

    /* 不存在的 key：应返回 Ok 且 value 为 NULL */
    err = latte_rocksdb_get_cf(rocksdb, "default", "nonexist", 8, &val, &vlen);
    assert(error_is_ok(err));
    assert(val == NULL && vlen == 0);

    /* 不存在的列族：应返回错误 */
    err = latte_rocksdb_get_cf(rocksdb, "no_cf", "rk1", 3, &val, &vlen);
    assert(!error_is_ok(err));
    error_delete(err);

    latte_rocksdb_close(rocksdb);
    printf("Read test passed\n");
    return 1;
}

/**
 * 测试 RocksDB Flush 功能
 *
 * Flush 的作用：
 * 1. 将 MemTable 中的数据持久化到 SSTable 文件
 * 2. 在切换 MemTable 后触发后台 flush 任务
 * 3. 确保数据落盘，提高数据持久性
 *
 * Flush 触发条件：
 * - 手动调用 Flush 接口
 * - MemTable 大小超过 write_buffer_size
 * - WAL 文件大小超过 max_total_wal_size
 *
 * Flush 与 WriteThread 的关系：
 * - PreprocessWrite 可能触发 ScheduleFlushes
 * - Flush 时需要等待 pending_memtable_writes_ 归零
 * - 这就是为什么 PreprocessWrite 必须在 EnterAsBatchGroupLeader 之前执行
 */
int test_rocksdb_flush() {
    printf("=== Testing RocksDB Flush functionality ===\n");

    // 创建测试数据库，使用较小的 write_buffer_size 以更容易触发 flush
    latte_rocksdb_t* rocksdb = test_create_rocksdb("./flush_test/");

    // 设置较小的写缓冲区大小，便于测试 flush
    LATTE_SET_DB_OPTION(rocksdb, set_write_buffer_size, 1 * 1024 * 1024);  // 1MB

    latte_error_t* error = latte_rocksdb_open(rocksdb);
    assert(error_is_ok(error));
    printf("Database opened successfully\n");

    // 写入第一组数据
    printf("Writing first batch of data...\n");
    for (int i = 0; i < 100; i++) {
        char key[64];
        char value[128];
        snprintf(key, sizeof(key), "batch1_key_%d", i);
        snprintf(value, sizeof(value), "batch1_value_%d_with_more_data", i);

        error = latte_rocksdb_write_cf(rocksdb, "default",
            key, strlen(key), value, strlen(value), NULL);
        assert(error_is_ok(error));
    }

    // 手动触发 flush
    printf("Triggering manual flush for column family 'default'...\n");
    error = latte_rocksdb_flush(rocksdb, "default");
    if (error_is_ok(error)) {
        printf("Flush initiated successfully for 'default' column family\n");
    } else {
        printf("Flush failed: %s\n", error ? error->state : "unknown error");
    }

    // 写入第二组数据到 memtable
    printf("Writing second batch of data...\n");
    for (int i = 0; i < 100; i++) {
        char key[64];
        char value[128];
        snprintf(key, sizeof(key), "batch2_key_%d", i);
        snprintf(value, sizeof(value), "batch2_value_%d_with_more_data", i);

        error = latte_rocksdb_write_cf(rocksdb, "default",
            key, strlen(key), value, strlen(value), NULL);
        assert(error_is_ok(error));
    }

    // 再次触发 flush
    printf("Triggering second flush...\n");
    error = latte_rocksdb_flush(rocksdb, "default");
    if (error_is_ok(error)) {
        printf("Second flush initiated successfully\n");
    }

    // 测试多列族 flush
    printf("Writing data to 'meta' column family...\n");
    for (int i = 0; i < 50; i++) {
        char key[64];
        char value[128];
        snprintf(key, sizeof(key), "meta_key_%d", i);
        snprintf(value, sizeof(value), "meta_value_%d_data", i);

        error = latte_rocksdb_write_cf(rocksdb, "meta",
            key, strlen(key), value, strlen(value), NULL);
        assert(error_is_ok(error));
    }

    // flush meta 列族
    printf("Flushing 'meta' column family...\n");
    error = latte_rocksdb_flush(rocksdb, "meta");
    if (error_is_ok(error)) {
        printf("Meta column family flush initiated successfully\n");
    }

    // 写入更多数据触发自动 flush（由于 write_buffer_size 较小）
    printf("Writing large data to trigger automatic flush...\n");
    for (int i = 0; i < 500; i++) {
        char key[64];
        char value[256];
        snprintf(key, sizeof(key), "large_key_%d", i);
        snprintf(value, sizeof(value),
            "large_value_%d_with_extended_data_to_fill_up_memtable_quickly", i);

        error = latte_rocksdb_write_cf(rocksdb, "default",
            key, strlen(key), value, strlen(value), NULL);
        if (!error_is_ok(error)) {
            printf("Write failed at iteration %d: %s\n", i,
                error ? error->state : "unknown error");
            error_delete(error);
            break;
        }
    }

    printf("All flush tests completed\n");
    latte_rocksdb_close(rocksdb);
    return 1;
}

/**
 * 测试 RocksDB Compact 功能
 *
 * Compact 的作用：
 * 1. 合并/重写 SST 文件，减少层级与文件数
 * 2. 回收已删除/过期数据占用的空间
 * 3. 可指定列族与 key 范围，NULL 表示全量 compact
 */
int test_rocksdb_compact() {
    printf("=== Testing RocksDB Compact functionality ===\n");

    latte_rocksdb_t* rocksdb = test_create_rocksdb("./compact_test/");
    latte_error_t* error = latte_rocksdb_open(rocksdb);
    assert(error_is_ok(error));

    // 写入数据
    for (int i = 0; i < 50; i++) {
        char key[64];
        char value[128];
        snprintf(key, sizeof(key), "compact_key_%d", i);
        snprintf(value, sizeof(value), "compact_value_%d", i);
        error = latte_rocksdb_write_cf(rocksdb, "default",
            key, strlen(key), value, strlen(value), NULL);
        assert(error_is_ok(error));
    }

    error = latte_rocksdb_flush(rocksdb, "default");
    assert(error_is_ok(error));

    // 全量 compact 指定列族（start_key/limit_key 传 NULL）
    printf("Triggering full compact for column family 'default'...\n");
    error = latte_rocksdb_compact_cf(rocksdb, "default", NULL, 0, NULL, 0);
    assert(error_is_ok(error));
    printf("Full compact completed for 'default'\n");

    // 按范围 compact（示例）
    error = latte_rocksdb_compact_cf(rocksdb, "default",
        "compact_key_0", 11, "compact_key_9", 11);
    assert(error_is_ok(error));
    printf("Range compact completed\n");

    // 不存在的列族应返回错误
    error = latte_rocksdb_compact_cf(rocksdb, "nonexistent", NULL, 0, NULL, 0);
    assert(!error_is_ok(error));
    error_delete(error);
    printf("Compact with invalid cf correctly returned error\n");

    printf("All compact tests completed\n");
    latte_rocksdb_close(rocksdb);
    return 1;
}

/**
 * 单测：close
 * 1) 仅 create 后 close（未 open）不崩溃；
 * 2) create + open 后 close 不崩溃，且可重复 close(NULL) 安全。
 */
int test_rocksdb_close() {
    printf("=== Testing RocksDB Close ===\n");

    /* 仅 create 未 open 即 close */
    latte_rocksdb_t* db1 = test_create_rocksdb("./close_test1/");
    latte_rocksdb_close(db1);
    printf("Close after create-only: OK\n");

    /* create + open 后 close */
    latte_rocksdb_t* db2 = test_create_rocksdb("./close_test2/");
    latte_error_t* err = latte_rocksdb_open(db2);
    assert(error_is_ok(err));
    latte_rocksdb_close(db2);
    printf("Close after open: OK\n");

    /* close(NULL) 应安全返回 */
    latte_rocksdb_close(NULL);
    printf("Close(NULL): OK\n");

    printf("All close tests completed\n");
    return 1;
}

// 线程函数：并发写入测试
void* concurrent_write_thread(void* arg) {
    latte_rocksdb_t* rocksdb = (latte_rocksdb_t*)arg;
    int thread_id = *(int*)arg;
    int num_writes = 1000;

    for (int i = 0; i < num_writes; i++) {
        char key[64];
        char value[128];
        snprintf(key, sizeof(key), "thread_%d_key_%d", thread_id, i);
        snprintf(value, sizeof(value), "value_from_thread_%d_iteration_%d", thread_id, i);

        latte_error_t* error = latte_rocksdb_write_cf(rocksdb, "default",
            key, strlen(key), value, strlen(value), NULL);

        if (!error_is_ok(error)) {
            printf("Thread %d write failed at iteration %d\n", thread_id, i);
            error_delete(error);
            break;
        }
    }

    return NULL;
}

// 测试并发写入以触发写线程的多角色协调
int test_concurrent_write() {
    printf("=== Testing concurrent writes to trigger STATE_GROUP_LEADER ===\n");

    latte_rocksdb_t* rocksdb = test_create_rocksdb("./concurrent_write_test/");

    // 修改配置以更容易触发写线程角色转换
    LATTE_SET_DB_OPTION(rocksdb, set_max_background_jobs, 4);
    LATTE_SET_DB_OPTION(rocksdb, set_max_background_flushes, 2);

    latte_error_t* error = latte_rocksdb_open(rocksdb);
    assert(error_is_ok(error));

    // 创建多个并发写线程
    int num_threads = 2;
    pthread_t threads[num_threads];
    int thread_ids[num_threads];

    for (int i = 0; i < num_threads; i++) {
        thread_ids[i] = i;
        int rc = pthread_create(&threads[i], NULL, concurrent_write_thread, rocksdb);
        if (rc != 0) {
            printf("Failed to create thread %d\n", i);
            return 0;
        }
    }

    // 等待所有线程完成
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("All %d threads completed writing\n", num_threads);
    latte_rocksdb_close(rocksdb);
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
        // test_cond("rocksdb write function",
        //     test_rockdb_write() == 1);
        test_cond("concurrent write test",
            test_concurrent_write() == 1);
        test_cond("rocksdb read test",
            test_rocksdb_read() == 1);
        test_cond("rocksdb flush test",
            test_rocksdb_flush() == 1);
        test_cond("rocksdb compact test",
            test_rocksdb_compact() == 1);
        test_cond("rocksdb close test",
            test_rocksdb_close() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}