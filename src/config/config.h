#ifndef __LATTE_CONFIG_H
#define __LATTE_CONFIG_H


#include "sds/sds.h"
#include "dict/dict.h"
#include "value/value.h"


typedef struct config_rule_t config_rule_t;

typedef int (*set_value_func)(void* data_ctx, void* new_value);
typedef void* (*get_value_func)(void* data_ctx);
typedef int (*check_value_func)(config_rule_t* rule, void* value, void* new_value);
typedef sds (*to_sds_func)(config_rule_t* rule ,void* data);
typedef void* (*load_value_func)(config_rule_t* rule, char** argv, int argc, char** error);
typedef int (*cmp_value_func)(config_rule_t* rule, void* value, void* new_value);
typedef int (*is_valid_func)(void* limit_arg, void* value);
typedef void (*delete_func)(void* data);
typedef struct config_rule_t {
    /* 是否可更新 */
    int flags; 
    /* 数据上下文 */
    void* data_ctx;
    /* 限制参数 */
    void* limit_arg;
    /* 设置数据 */
    set_value_func set_value;
    /* 获取数据 */
    get_value_func get_value;
    /* 比较值 */
    cmp_value_func cmp_value;
    /* 限制参数 */
    is_valid_func is_valid;
    /* 前期判断范围是否可更新 */
    check_value_func check_value;
    /** 配合写入文件 */
    to_sds_func to_sds;
    /** 加载数据时，类型转换 */
    load_value_func load_value;
    /* 默认值 */
    sds default_value;
    /* 删除数据 */
    delete_func delete_value;
    /* 删除限制参数 */
    delete_func delete_limit;
} config_rule_t;

typedef struct config_manager_t {
    /* dict<sds, config_rule_t> */
    dict_t* rules;                  
} config_manager_t;

/* 管理器相关函数 */
/* 创建管理器 */
config_manager_t* config_manager_new(void);
void config_manager_delete(config_manager_t* manager);

int config_init_all_data(config_manager_t* manager);

/* 添加规则 */
int config_add_rule(config_manager_t* manager, char* key,config_rule_t* rule);
/* 获取规则 */
config_rule_t* config_get_rule(config_manager_t* manager, char* key); 
/* 删除规则 */
int config_remove_rule(config_manager_t* manager, char* key); 
/* 加载文件 */
int config_load_file(config_manager_t* manager, char* file);
int config_load_string(config_manager_t* manager, char* str, size_t len);
int config_load_argv(config_manager_t* manager,  char** argv, int argc);
int config_save_file(config_manager_t* manager, char* file);
/* 设置值 */
int config_set_value(config_manager_t* manager, char** argv, int argc, char** err);
/* 获取值 */
void* config_get_value(config_manager_t* manager, char* key);
sds config_rule_to_sds(config_manager_t* manager, char* key);


/* 规则相关函数 */
/* 创建规则 */
config_rule_t* config_rule_new(int flags, 
    void* data_ctx, 
    set_value_func* set_value, 
    get_value_func* get_value, 
    check_value_func* check_value, 
    load_value_func* load_value,
    cmp_value_func* cmp_value,
    is_valid_func* is_valid,
    to_sds_func* to_sds, 
    void* limit_arg,
    delete_func* delete_limit,
    delete_func* delete_value,
    sds default_value  
);
/* 删除规则 */
void config_rule_delete(config_rule_t* rule);

/* 数值类型规则 */
typedef struct numeric_data_limit_t {
    long long lower_bound; /* The lower bound of this numeric value */
    long long upper_bound; /* The upper bound of this numeric value */
} numeric_data_limit_t;
config_rule_t* config_rule_new_numeric_rule(int flags, long long* data_ctx, 
    long long lower_bound, long long upper_bound, check_value_func* check_value,  long long default_value);

/* 字符串类型规则 */
typedef struct sds_data_limit_t {
    
} sds_data_limit_t;

config_rule_t* config_rule_new_sds_rule(int flags, sds* data_ctx, 
    check_value_func* check_value, sds default_value);


/* 枚举类型规则 */
/* Enum Configs contain an array of configEnum objects that match a string with an integer. */
typedef struct config_enum_t {
    char *name;
    int val;
} config_enum_t;
typedef struct enum_data_limit_t {
    config_enum_t *enum_value;
} enum_data_limit_t;
config_rule_t* config_rule_new_enum_rule(int flags, void* data_ctx, 
    config_enum_t* limit, check_value_func* check_value, sds default_value);

/* 布尔类型规则 */
config_rule_t* config_rule_new_bool_rule(int flags, int* data_ctx, 
    check_value_func* check_value, int default_value);


/* 数组类型规则 */
typedef struct array_data_limit_t {
    int size;
} array_data_limit_t;
config_rule_t* config_rule_new_sds_array_rule(int flags, void* data_ctx, 
    check_value_func* check_value, int size, sds default_value);

/* int64数组*/
/* enum数组 */
/* bool数组 */
/* 自定义数组 */



/* 多段数组追加规则 */
// config_rule_t* config_rule_new_append_array_rule(int flags, void* data_ctx, 
//     check_value_func* check_value, sds default_value);

/* map<sds, sds>类型规则 */   
config_rule_t* config_rule_new_map_sds_sds_rule(int flags, void* data_ctx, 
    check_value_func* check_value, char** keys, sds default_value);

/* 多段map<sds,sds>属性插入规则 */   
config_rule_t* config_rule_new_append_map_sds_sds_rule(int flags, void* data_ctx, 
    check_value_func* check_value, char** keys, sds default_value);


sds config_diff_file(config_manager_t* manager, char* filename);
//读取文件以后移走 
sds read_file_to_sds(const char *filename);
#endif