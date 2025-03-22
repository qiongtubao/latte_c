
#ifndef __LATTE_CONFIG_H
#define __LATTE_CONFIG_H

#include "sds/sds.h"
#include "dict/dict.h"
#include "value/value.h"
typedef struct config_rule_t {
    value_t value;
    /** 前期判断范围是否可更新 */
    int (*check_update)(struct config_rule_t* rule, void* old_value, void *new_value);
    /** 配合写入文件 */
    sds_t (*to_sds)(sds_t config, char* key,struct config_rule_t* rule);
    /** 加载数据时，类型转换 */
    int (*load)(struct config_rule_t* rule, char** argv, int argc);
} config_rule_t;

typedef struct config_manager_t {
    dict_t* rules;
} config_manager_t;
struct config_manager_t* config_manager_new();
void config_manager_delete(config_manager_t* c);
config_rule_t* config_get_rule(config_manager_t* c, char* key);
int config_register_rule(config_manager_t* c, sds_t key, config_rule_t* rule);


int config_set_argv(config_rule_t* c, char** argv, int argc);
sds_t write_config_to_sds(config_manager_t* c);


/** load config **/
int load_config_from_string(config_manager_t* config, char* configstr, size_t configstrlen);
int load_config_from_argv(config_manager_t* config, char** argv, int argc);
int load_config_from_file(config_manager_t* c, char *filename);

/* num attribute */
sds_t write_config_int64(sds_t config, char* key, config_rule_t* rule);
void value_delete_int64(void* value);
int load_config_int64(config_rule_t* rule, char** argv, int argc);
int64_t config_get_int64(config_manager_t* c, char* key);
int config_set_int64(config_manager_t* c, char* key, int64_t value);

/* sds_t attribute */
sds_t write_config_sds(sds_t config, char* key, config_rule_t* rule);
void value_delete_sds(void* value);
int load_config_sds(config_rule_t* rule, char** argv, int argc);
sds_t config_get_sds(config_manager_t* c, char* key);
int config_set_sds(config_manager_t* c, char* key, sds_t value);

// /* array attribute */
// int config_update_array(config_rule_t* rule, void* old_value, void* new_value);
sds_t write_config_sds_array(sds_t config, char* key, config_rule_t* rule);
int load_config_sds_array(config_rule_t* rule, char** argv, int argc);
vector_t* config_get_array(config_manager_t* c, char* key);
int config_set_array(config_manager_t* c, char* key, vector_t* value);
// void value_delete_array(void* value);
// struct arraySds* config_get_array(config_rule_t* c, char* key);

// /* object attribute */
// int config_update_object(config_rule_t* rule, void* old_value, void* new_value);


#define LL_CONFIG_INIT(v) \
    { .check_update = NULL, \
      .to_sds = write_config_int64,\
      .load = load_config_int64,\
      .value.type = VALUE_INT,\
      .value.value.i64_value = v }

#define SDS_CONFIG_INIT(v) \
    { .check_update = NULL, \
    .to_sds = write_config_sds, \
    .load = load_config_sds, \
    .value.type=VALUE_CONSTANT_CHAR,\
    .value.value.sds_value = v }

#define SDS_ARRAY_CONFIG_INIT(v) \
{ .check_update = NULL,\
  .to_sds = write_config_sds_array,\
  .load = load_config_sds_array,\
  .value.type=VALUE_CONSTANT_CHAR,\
  .value.value.sds_value = v}
#endif

