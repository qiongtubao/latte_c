/**
 * @file latte_rocksdb.h
 * @brief RocksDB 封装模块接口
 *        提供基于列族（Column Family）的 RocksDB 打开、读写、刷盘、压缩和关闭操作
 */
#ifndef __LATTE_ROCKSDB_H__
#define __LATTE_ROCKSDB_H__


#include <rocksdb/c.h>
#include "dict/dict.h"
#include "sds/sds.h"
#include "error/error.h"
#include "dict/dict_plugins.h"

/**
 * @brief RocksDB 数据库实例封装
 *        包含数据库路径、rocksdb 实例、各类选项及列族元数据字典
 */
typedef struct latte_rocksdb_t {
    sds db_path;                          /**< 数据库目录路径（sds 字符串） */
    rocksdb_t* db;                        /**< RocksDB 实例指针 */
    rocksdb_options_t* db_opts;           /**< 数据库级选项 */
    dict_t* column_families_metas;        /**< 列族元数据字典（key: 列族名 sds） */
    rocksdb_writeoptions_t* write_opts;   /**< 写选项 */
    rocksdb_readoptions_t* read_opts;     /**< 读选项 */
    rocksdb_compactoptions_t* compact_opts; /**< 压缩选项 */
    rocksdb_flushoptions_t* flush_opts;   /**< 刷盘选项 */
} latte_rocksdb_t;

/**
 * @brief 列族元数据
 *        记录列族名称、专属选项及打开后的 handle
 */
typedef struct latte_rocksdb_column_family_meta_t {
    sds name;                                      /**< 列族名称（sds） */
    rocksdb_options_t* cf_opts;                    /**< 列族级选项 */
    rocksdb_column_family_handle_t* handle;        /**< 列族句柄（open 后有效） */
} latte_rocksdb_column_family_meta_t;


/**
 * @brief 设置 DB 级选项的便捷宏
 * @param rocksdb latte_rocksdb_t 实例
 * @param option  选项名（去掉 rocksdb_options_ 前缀）
 * @param ...     选项参数
 */
#define LATTE_SET_DB_OPTION(rocksdb, option, ...) \
    do { \
        rocksdb_options_##option(rocksdb->db_opts, __VA_ARGS__); \
    } while (0)

/**
 * @brief 设置读选项的便捷宏
 * @param rocksdb latte_rocksdb_t 实例
 * @param option  选项名（去掉 rocksdb_readoptions_ 前缀）
 * @param ...     选项参数
 */
#define LATTE_SET_DB_READ_OPTION(rocksdb, option, ...) \
    do { \
        rocksdb_readoptions_##option(rocksdb->read_opts, __VA_ARGS__); \
    } while (0)

/**
 * @brief 设置写选项的便捷宏
 * @param rocksdb latte_rocksdb_t 实例
 * @param option  选项名（去掉 rocksdb_writeoptions_ 前缀）
 * @param ...     选项参数
 */
#define LATTE_SET_DB_WRITE_OPTION(rocksdb, option, ...) \
    do { \
        rocksdb_writeoptions_##option(rocksdb->write_opts, __VA_ARGS__); \
    } while (0)

/**
 * @brief 通过 meta 指针设置列族选项的便捷宏
 * @param meta   latte_rocksdb_column_family_meta_t 指针
 * @param option 选项名（去掉 rocksdb_options_ 前缀）
 * @param ...    选项参数
 */
#define LATTE_ROCKSDB_SET_CF_OPTION(meta, option,  ...) \
    do { \
        rocksdb_options_##option(meta->cf_opts, __VA_ARGS__); \
    } while (0)

/**
 * @brief 通过列族名称查找并设置列族选项的便捷宏
 * @param rocksdb latte_rocksdb_t 实例
 * @param name    列族名称（C 字符串）
 * @param option  选项名（去掉 rocksdb_options_set_ 前缀）
 * @param ...     选项参数
 */
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


/**
 * @brief 创建 latte_rocksdb_t 实例并初始化各选项（不打开数据库）
 * @param dir_name 数据库目录路径（sds）
 * @return 新建的 latte_rocksdb_t 指针
 */
latte_rocksdb_t* latte_rocksdb_create(sds dir_name);

/**
 * @brief 打开 RocksDB 数据库（含所有已注册列族）
 *        调用前须先通过 latte_rocksdb_add_column_family 注册所有列族
 * @param db 已创建的 latte_rocksdb_t 实例
 * @return &Ok 成功；错误时返回错误对象
 */
latte_error_t* latte_rocksdb_open(latte_rocksdb_t* db);

/**
 * @brief 关闭 RocksDB 并释放所有相关资源（列族 handle/opts、DB opts、路径等）
 *        传入 NULL 时安全跳过
 * @param db 要关闭的 latte_rocksdb_t 实例
 */
void latte_rocksdb_close(latte_rocksdb_t* db);

/**
 * @brief 注册一个新列族（open 前调用）
 *        自动创建列族专属 options，handle 在 open 后填充
 * @param rocksdb 目标数据库实例
 * @param name    列族名称（C 字符串）
 * @return 新建的列族元数据指针
 */
latte_rocksdb_column_family_meta_t* latte_rocksdb_add_column_family(latte_rocksdb_t* rocksdb,
        char* name);

/**
 * @brief 批量写入指定列族
 *        参数列表为 (key, key_len, value, value_len) 四元组序列，以 NULL key 结尾
 * @param rocksdb  目标数据库实例
 * @param cf_name  列族名称
 * @param ...      (const char* key, size_t key_len, const char* value, size_t value_len, ..., NULL)
 * @return &Ok 成功；错误时返回错误对象
 */
latte_error_t* latte_rocksdb_write_cf(latte_rocksdb_t* rocksdb, const char* cf_name, ...);

/**
 * @brief 从指定列族读取 key 对应的 value
 *        成功时 *out_value 为 RocksDB 分配的内存，调用方须用 rocksdb_free(*out_value) 释放；
 *        key 不存在时 *out_value 为 NULL，*out_value_len 为 0，仍返回 &Ok
 * @param rocksdb       目标数据库实例
 * @param cf_name       列族名称
 * @param key           查询 key
 * @param key_len       key 字节长度
 * @param out_value     输出 value 指针（由 RocksDB 分配）
 * @param out_value_len 输出 value 字节长度
 * @return &Ok 成功；列族不存在或读取失败返回错误对象
 */
latte_error_t* latte_rocksdb_get_cf(latte_rocksdb_t* rocksdb, const char* cf_name,
    const char* key, size_t key_len, char** out_value, size_t* out_value_len);

/**
 * @brief 将指定列族的 MemTable 刷盘到 SST 文件
 * @param rocksdb  目标数据库实例
 * @param cf_name  列族名称
 * @return &Ok 成功；列族不存在或刷盘失败返回错误对象
 */
latte_error_t* latte_rocksdb_flush(latte_rocksdb_t* rocksdb, const char* cf_name);

/**
 * @brief 对指定列族执行范围或全量 Compaction
 *        start_key/limit_key 均为 NULL 时执行全量压缩
 * @param rocksdb       目标数据库实例
 * @param cf_name       列族名称
 * @param start_key     压缩起始 key（NULL 表示从最小 key）
 * @param start_key_len 起始 key 长度（0 表示全量）
 * @param limit_key     压缩结束 key（NULL 表示到最大 key，不包含）
 * @param limit_key_len 结束 key 长度（0 表示全量）
 * @return &Ok 成功；列族不存在返回错误对象
 */
latte_error_t* latte_rocksdb_compact_cf(latte_rocksdb_t* rocksdb, const char* cf_name,
    const char* start_key, size_t start_key_len,
    const char* limit_key, size_t limit_key_len);

#endif
