/**
 * @file config.h
 * @brief 配置管理器接口
 *        提供基于规则（config_rule_t）的配置系统：
 *        - 支持数值、字符串、枚举、布尔、数组、map 等多种类型规则
 *        - 支持从文件、字符串、命令行参数加载配置
 *        - 支持运行时读写、持久化保存、差异对比
 *        所有规则统一存储在字典中，通过 key 字符串索引。
 */
#ifndef __LATTE_CONFIG_H
#define __LATTE_CONFIG_H


#include "sds/sds.h"
#include "dict/dict.h"
#include "value/value.h"

/** @brief 配置标志：禁止通过 config_set_value 运行时修改 */
#define CONFIG_FLAG_DISABLE_WRITE 0x00000001
/** @brief 配置标志：禁止通过 config_save_file 持久化保存 */
#define CONFIG_FLAG_DISABLE_SAVE 0x00000002


typedef struct config_rule_t config_rule_t;

/**
 * @brief 设置值回调函数类型
 * @param data_ctx 数据上下文指针（指向实际存储变量的地址）
 * @param new_value 新值指针
 * @return 成功返回 0；失败返回非零
 */
typedef int (*set_value_func)(void* data_ctx, void* new_value);

/**
 * @brief 获取值回调函数类型
 * @param data_ctx 数据上下文指针（指向实际存储变量的地址）
 * @return 当前值指针
 */
typedef void* (*get_value_func)(void* data_ctx);

/**
 * @brief 检查值是否可更新的回调函数类型
 *        在实际写入之前调用，用于前置校验（如范围检查、状态检查等）。
 * @param rule      当前配置规则指针
 * @param value     当前旧值指针
 * @param new_value 待写入的新值指针
 * @return 允许更新返回 0；拒绝返回非零
 */
typedef int (*check_value_func)(config_rule_t* rule, void* value, void* new_value);

/**
 * @brief 将配置值序列化为 sds 字符串的回调函数类型
 *        用于持久化（config_save_file）时将值转换为文本格式。
 * @param rule 当前配置规则指针
 * @param key  配置键字符串
 * @param data 值指针
 * @return 新建的 sds 字符串（调用方负责释放）
 */
typedef sds (*to_sds_func)(config_rule_t* rule, char* key, void* data);

/**
 * @brief 从命令行/文件参数加载并转换值的回调函数类型
 * @param rule  当前配置规则指针
 * @param argv  参数数组
 * @param argc  参数数量
 * @param error 输出：错误描述字符串指针（失败时设置）
 * @return 转换后的值指针；失败返回 NULL 并设置 *error
 */
typedef void* (*load_value_func)(config_rule_t* rule, char** argv, int argc, char** error);

/**
 * @brief 比较新旧值的回调函数类型
 * @param rule      当前配置规则指针
 * @param value     旧值指针
 * @param new_value 新值指针
 * @return 相等返回 0；不等返回非零
 */
typedef int (*cmp_value_func)(config_rule_t* rule, void* value, void* new_value);

/**
 * @brief 校验值是否在 limit_arg 限制范围内的回调函数类型
 * @param limit_arg 限制参数指针（如 numeric_data_limit_t 表示数值范围）
 * @param value     待校验的值指针
 * @return 合法返回 1；非法返回 0
 */
typedef int (*is_valid_func)(void* limit_arg, void* value);

/**
 * @brief 通用释放回调函数类型
 * @param data 要释放的数据指针
 */
typedef void (*delete_func)(void* data);

/**
 * @brief 配置规则结构体
 *        描述单条配置项的全部元信息：类型回调、限制范围、默认值、生命周期管理。
 */
typedef struct config_rule_t {
    int flags;                   /**< 配置标志（CONFIG_FLAG_DISABLE_WRITE / DISABLE_SAVE） */
    void* data_ctx;              /**< 数据上下文指针，指向实际存储变量的地址 */
    void* limit_arg;             /**< 限制参数指针（如数值范围、枚举表等），可为 NULL */
    set_value_func set_value;    /**< 设置数据回调 */
    get_value_func get_value;    /**< 获取数据回调 */
    cmp_value_func cmp_value;    /**< 比较新旧值回调 */
    is_valid_func is_valid;      /**< 校验值合法性回调 */
    check_value_func check_value;/**< 前置更新检查回调（写入前判断是否可更新） */
    to_sds_func to_sds;          /**< 值序列化为 sds 字符串回调（用于持久化） */
    load_value_func load_value;  /**< 从参数加载并类型转换回调 */
    sds default_value;           /**< 默认值（sds 字符串格式，用于初始化） */
    delete_func delete_value;    /**< 释放 data_ctx 指向数据的析构函数 */
    delete_func delete_limit;    /**< 释放 limit_arg 的析构函数 */
} config_rule_t;

/**
 * @brief 配置管理器结构体
 *        以字典形式管理所有配置规则（key 为配置名，value 为 config_rule_t*）。
 */
typedef struct config_manager_t {
    dict_t* rules; /**< 配置规则字典：dict<sds, config_rule_t*> */
} config_manager_t;

/* ---- 管理器相关函数 ---- */

/**
 * @brief 创建配置管理器
 * @return 新建的 config_manager_t 指针；内存分配失败返回 NULL
 */
config_manager_t* config_manager_new(void);

/**
 * @brief 销毁配置管理器及所有规则
 * @param manager 要销毁的管理器指针（NULL 时安全跳过）
 */
void config_manager_delete(config_manager_t* manager);

/**
 * @brief 初始化管理器中所有规则的默认值
 *        遍历所有规则，将 default_value 加载并写入对应的 data_ctx。
 * @param manager 配置管理器指针
 * @return 全部成功返回 0；任意规则失败返回非零
 */
int config_init_all_data(config_manager_t* manager);

/**
 * @brief 向管理器添加配置规则
 * @param manager 配置管理器指针
 * @param key     配置键字符串（将被复制存储）
 * @param rule    配置规则指针
 * @return 成功返回 0（DICT_OK）；失败返回非零
 */
int config_add_rule(config_manager_t* manager, char* key, config_rule_t* rule);

/**
 * @brief 获取指定 key 的配置规则
 * @param manager 配置管理器指针
 * @param key     配置键字符串
 * @return 找到返回 config_rule_t 指针；未找到返回 NULL
 */
config_rule_t* config_get_rule(config_manager_t* manager, char* key);

/**
 * @brief 删除指定 key 的配置规则（调用规则析构函数并从字典移除）
 * @param manager 配置管理器指针
 * @param key     配置键字符串
 * @return 成功返回 0；未找到返回非零
 */
int config_remove_rule(config_manager_t* manager, char* key);

/**
 * @brief 从文件加载配置（解析文件中每行的键值参数并应用规则）
 * @param manager 配置管理器指针
 * @param file    配置文件路径
 * @return 成功返回 0；解析或应用失败返回非零
 */
int config_load_file(config_manager_t* manager, char* file);

/**
 * @brief 从字符串加载配置（解析字符串中的键值参数并应用规则）
 * @param manager 配置管理器指针
 * @param str     配置字符串指针
 * @param len     配置字符串长度
 * @return 成功返回 0；解析或应用失败返回非零
 */
int config_load_string(config_manager_t* manager, char* str, size_t len);

/**
 * @brief 从命令行参数加载配置
 * @param manager 配置管理器指针
 * @param argv    参数数组
 * @param argc    参数数量
 * @return 成功返回 0；解析或应用失败返回非零
 */
int config_load_argv(config_manager_t* manager, char** argv, int argc);

/**
 * @brief 将所有可持久化（无 DISABLE_SAVE 标志）的配置保存到文件
 * @param manager 配置管理器指针
 * @param file    目标文件路径
 * @return 成功返回 0；写入失败返回非零
 */
int config_save_file(config_manager_t* manager, char* file);

/**
 * @brief 通过参数数组设置配置值（运行时动态修改）
 *        argv[0] 为配置键，argv[1..] 为值参数；
 *        若规则设置了 CONFIG_FLAG_DISABLE_WRITE，将拒绝修改并设置错误信息。
 * @param manager 配置管理器指针
 * @param argv    参数数组（argv[0] = key）
 * @param argc    参数数量
 * @param err     输出：错误描述字符串（失败时设置，成功时不修改）
 * @return 成功返回 0；失败返回非零
 */
int config_set_value(config_manager_t* manager, char** argv, int argc, char** err);

/**
 * @brief 获取指定 key 的当前配置值
 * @param manager 配置管理器指针
 * @param key     配置键字符串
 * @return 找到返回值指针；未找到或无 get_value 回调返回 NULL
 */
void* config_get_value(config_manager_t* manager, char* key);

/**
 * @brief 将指定 key 的当前值序列化为 sds 字符串
 * @param manager 配置管理器指针
 * @param key     配置键字符串
 * @return 新建的 sds 字符串（调用方负责释放）；未找到返回 NULL
 */
sds config_rule_to_sds(config_manager_t* manager, char* key);


/* ---- 规则相关函数 ---- */

/**
 * @brief 创建通用配置规则（完整参数版）
 * @param flags         配置标志（CONFIG_FLAG_DISABLE_WRITE / DISABLE_SAVE）
 * @param data_ctx      数据上下文指针
 * @param set_value     设置值回调
 * @param get_value     获取值回调
 * @param check_value   前置更新检查回调
 * @param load_value    从参数加载回调
 * @param cmp_value     值比较回调
 * @param is_valid      合法性校验回调
 * @param to_sds        序列化为 sds 回调
 * @param limit_arg     限制参数指针
 * @param delete_limit  limit_arg 析构回调
 * @param delete_value  data_ctx 数据析构回调
 * @param default_value 默认值（sds 格式）
 * @return 新建的 config_rule_t 指针；内存分配失败返回 NULL
 */
config_rule_t* config_rule_new(int flags,
    void* data_ctx,
    set_value_func set_value,
    get_value_func get_value,
    check_value_func check_value,
    load_value_func load_value,
    cmp_value_func cmp_value,
    is_valid_func is_valid,
    to_sds_func to_sds,
    void* limit_arg,
    delete_func delete_limit,
    delete_func delete_value,
    sds default_value
);

/**
 * @brief 销毁配置规则（调用 delete_value/delete_limit 并释放规则本身）
 * @param rule 要销毁的规则指针（NULL 时安全跳过）
 */
void config_rule_delete(config_rule_t* rule);

/* ---- 数值类型规则 ---- */

/**
 * @brief 数值类型限制参数结构体
 *        描述 long long 类型配置项的合法值范围。
 */
typedef struct numeric_data_limit_t {
    long long lower_bound; /**< 数值下界（含） */
    long long upper_bound; /**< 数值上界（含） */
} numeric_data_limit_t;

/**
 * @brief 创建数值类型配置规则
 * @param flags         配置标志
 * @param data_ctx      指向 long long 变量的指针
 * @param lower_bound   数值下界
 * @param upper_bound   数值上界
 * @param check_value   前置更新检查回调（NULL 表示不检查）
 * @param default_value 默认值
 * @return 新建的 config_rule_t 指针；内存分配失败返回 NULL
 */
config_rule_t* config_rule_new_numeric_rule(int flags, long long* data_ctx,
    long long lower_bound, long long upper_bound, check_value_func check_value, long long default_value);

/* ---- 字符串类型规则 ---- */

/**
 * @brief 字符串类型限制参数结构体（当前为空，预留扩展）
 */
typedef struct sds_data_limit_t {

} sds_data_limit_t;

/**
 * @brief 创建字符串（sds）类型配置规则
 * @param flags         配置标志
 * @param data_ctx      指向 sds 变量的指针
 * @param check_value   前置更新检查回调（NULL 表示不检查）
 * @param default_value 默认值（sds 格式）
 * @return 新建的 config_rule_t 指针；内存分配失败返回 NULL
 */
config_rule_t* config_rule_new_sds_rule(int flags, sds* data_ctx,
    check_value_func check_value, sds default_value);

/* ---- 枚举类型规则 ---- */

/**
 * @brief 枚举配置项结构体
 *        将字符串名称映射到整型值，配合 enum_data_limit_t 使用。
 */
typedef struct config_enum_t {
    char *name; /**< 枚举名称字符串（如 "always"、"no" 等） */
    int val;    /**< 对应的整型值 */
} config_enum_t;

/**
 * @brief 枚举类型限制参数结构体
 */
typedef struct enum_data_limit_t {
    config_enum_t *enum_value; /**< 枚举映射表数组（以 {NULL, 0} 结尾） */
} enum_data_limit_t;

/**
 * @brief 创建枚举类型配置规则
 * @param flags         配置标志
 * @param data_ctx      指向整型变量的指针（存储枚举 val）
 * @param limit         枚举映射表指针
 * @param check_value   前置更新检查回调（NULL 表示不检查）
 * @param default_value 默认值（枚举名称字符串）
 * @return 新建的 config_rule_t 指针；内存分配失败返回 NULL
 */
config_rule_t* config_rule_new_enum_rule(int flags, void* data_ctx,
    config_enum_t* limit, check_value_func check_value, sds default_value);

/* ---- 布尔类型规则 ---- */

/**
 * @brief 创建布尔类型配置规则
 * @param flags         配置标志
 * @param data_ctx      指向 bool 变量的指针
 * @param check_value   前置更新检查回调（NULL 表示不检查）
 * @param default_value 默认值（0 = false，非零 = true）
 * @return 新建的 config_rule_t 指针；内存分配失败返回 NULL
 */
config_rule_t* config_rule_new_bool_rule(int flags, bool* data_ctx,
    check_value_func check_value, int default_value);

/* ---- 数组类型规则 ---- */

/**
 * @brief 数组类型限制参数结构体
 */
typedef struct array_data_limit_t {
    size_t size; /**< 数组最大元素数量限制 */
} array_data_limit_t;

/**
 * @brief 创建 sds 字符串数组类型配置规则
 * @param flags         配置标志
 * @param data_ctx      指向 sds 数组的指针
 * @param check_value   前置更新检查回调（NULL 表示不检查）
 * @param size          数组最大元素数量
 * @param default_value 默认值（sds 格式）
 * @return 新建的 config_rule_t 指针；内存分配失败返回 NULL
 */
config_rule_t* config_rule_new_sds_array_rule(int flags, void* data_ctx,
    check_value_func check_value, int size, sds default_value);

/* int64数组 */
/* enum数组 */
/* bool数组 */
/* 自定义数组 */


/* 多段数组追加规则（预留，暂未实现） */
// config_rule_t* config_rule_new_append_array_rule(int flags, void* data_ctx,
//     check_value_func check_value, sds default_value);

/* ---- map<sds, sds> 类型规则 ---- */

/**
 * @brief 创建 map<sds, sds> 类型配置规则
 *        支持以多个指定 key 字段组成的映射配置项。
 * @param flags         配置标志
 * @param data_ctx      指向 map 数据的指针
 * @param check_value   前置更新检查回调（NULL 表示不检查）
 * @param keys          合法 key 列表数组（以 NULL 结尾）
 * @param default_value 默认值（sds 格式）
 * @return 新建的 config_rule_t 指针；内存分配失败返回 NULL
 */
config_rule_t* config_rule_new_map_sds_sds_rule(int flags, void* data_ctx,
    check_value_func check_value, char** keys, sds default_value);

/**
 * @brief 创建多段追加 map<sds,sds> 类型规则
 *        允许通过多次 config_set_value 追加条目到同一个 map 配置项。
 * @param flags         配置标志
 * @param data_ctx      指向 map 数据的指针
 * @param check_value   前置更新检查回调（NULL 表示不检查）
 * @param keys          合法 key 列表数组（以 NULL 结尾）
 * @param default_value 默认值（sds 格式）
 * @return 新建的 config_rule_t 指针；内存分配失败返回 NULL
 */
config_rule_t* config_rule_new_append_map_sds_sds_rule(int flags, void* data_ctx,
    check_value_func check_value, char** keys, sds default_value);

/**
 * @brief 对比当前配置与指定文件内容的差异，返回差异描述字符串
 * @param manager  配置管理器指针
 * @param filename 对比文件路径
 * @return 新建的 sds 差异描述字符串（调用方负责释放）；无差异或失败返回 NULL
 */
sds config_diff_file(config_manager_t* manager, char* filename);

/**
 * @brief 读取文件内容到 sds 字符串（读取后应由调用方负责移走/释放）
 * @param filename 文件路径
 * @return 新建的 sds 字符串（调用方负责 sdsfree）；失败返回 NULL
 */
sds read_file_to_sds(const char *filename);

#endif
