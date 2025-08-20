#include "config.h"
#include "zmalloc/zmalloc.h"
#include "dict/dict.h"
#include "dict/dict_plugins.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "utils/utils.h"
#include "log/log.h"
#define UNUSED(x) (void)(x)

void dict_delete_config_rule(dict_t* dict, void* rule) {
    UNUSED(dict);
    config_rule_delete(rule);
}


dict_func_t rule_dict_type_func = {
    .hashFunction = dict_char_hash,
    .keyCompare = dict_char_key_compare,
    .keyDestructor = dict_sds_destructor,
    .valDestructor = dict_delete_config_rule,
};

config_manager_t* config_manager_new(void) {
    config_manager_t* manager = zmalloc(sizeof(config_manager_t));
    manager->rules = dict_new(&rule_dict_type_func);
    return manager;
}

void config_manager_delete(config_manager_t* manager) {
    dict_delete(manager->rules);
    zfree(manager);
}

int config_init_all_data(config_manager_t* manager) {
    latte_iterator_t* iter = dict_get_latte_iterator(manager->rules);
    int result = 0;
    sds_t* argv = NULL;
    int argc = 0;

    while (latte_iterator_has_next(iter)) {
        latte_pair_t* pair = latte_iterator_next(iter);
        latte_assert_with_info(pair != NULL, "pair is NULL");
        sds key = latte_pair_key(pair);
        latte_assert_with_info(key != NULL, "key is NULL");
        config_rule_t* rule = latte_pair_value(pair);
        latte_assert_with_info(rule != NULL, "%s rule is NULL", key);
        if (rule->default_value == NULL) {
            result += 1;
            continue;
        }
        argc = 0;
        argv = sds_split_args(rule->default_value, &argc);
        if (argv == NULL) {
            goto error;
        }
        char* err = NULL;
        void* value = rule->load_value(rule, argv, argc, &err);
        if (err != NULL) {
            LATTE_LIB_LOG(LOG_ERROR, "error: load value failed, %s", err);
            goto error;
        }
        if (!rule->set_value(rule->data_ctx, value)) {
            LATTE_LIB_LOG(LOG_ERROR, "error: set value failed");
            goto error;
        }
        sds_free_splitres(argv, argc);
        argv = NULL;
        argc = 0;
        result += 1;
    }
    goto end;
error:
    result = 0;
end:    
    latte_iterator_delete(iter);
    if (argv != NULL) {
        sds_free_splitres(argv, argc);
    }
    return result;
}

int config_add_rule(config_manager_t* manager, char* key, config_rule_t* rule) {
    return dict_add(manager->rules, sds_new(key), rule);
}

config_rule_t* config_get_rule(config_manager_t* manager, char* key) {
    return dict_fetch_value(manager->rules, key);
}

int config_remove_rule(config_manager_t* manager, char* key) {
    return dict_delete_key(manager->rules, key);
}

/**
 * 读取文件内容到 SDS 字符串
 * @param filename 文件路径
 * @return 成功返回 sds 字符串，失败返回 NULL
 */
sds read_file_to_sds(const char *filename) {
    FILE *fp = NULL;
    sds config = NULL;
    long file_size;
    size_t bytes_read;

    // 1. 打开文件（二进制模式，避免换行符转换）
    fp = fopen(filename, "rb");  // 使用 "rb" 模式，可移植性更好
    if (!fp) {
        LATTE_LIB_LOG(LOG_ERROR, "error: can't open config file '%s': %s", filename, strerror(errno));
        goto cleanup;
    }

    // 2. 获取文件大小（可选，用于预分配内存，提升性能）
    if (fseek(fp, 0, SEEK_END) != 0) {
        LATTE_LIB_LOG(LOG_ERROR, "error: can't seek config file '%s': %s", filename, strerror(errno));
        goto cleanup;
    }
    file_size = ftell(fp);
    if (file_size < 0) {
        LATTE_LIB_LOG(LOG_ERROR, "error: can't get file size '%s': %s", filename, strerror(errno));
        goto cleanup;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        LATTE_LIB_LOG(LOG_ERROR, "error: can't seek config file '%s': %s", filename, strerror(errno));
        goto cleanup;
    }

    // 3. 预分配 SDS 内存（避免多次 realloc）
    config = sds_empty_len(file_size);  // 分配足够空间
    if (!config) {
        LATTE_LIB_LOG(LOG_ERROR, "error: can't allocate memory for config file '%s': %s", filename, strerror(errno));
        goto cleanup;
    }

    // 4. 一次性读取整个文件
    bytes_read = fread(config, 1, file_size, fp);
    if (ferror(fp)) {
        LATTE_LIB_LOG(LOG_ERROR, "error: can't read config file '%s': %s", filename, strerror(errno));
        sds_delete(config);
        config = NULL;
        goto cleanup;
    }

    // 5. 调整 SDS 长度（实际读取的字节数）
    config[bytes_read] = '\0';  // SDS 要求以 \0 结尾
    sds_set_len(config, bytes_read);

cleanup:
    if (fp) fclose(fp);
    return config;
}

int _config_load_string(config_manager_t* manager, dict_t* config_value, char* str, size_t len);
int _config_load_file(config_manager_t* manager, dict_t* config_value, char* filename) {
    sds config = read_file_to_sds(filename);
    if (config == NULL) {
        printf(
            "Fatal error, can't open config file '%s': %s\n",
            filename, strerror(errno));
        exit(1);
    }
    int result = _config_load_string(manager, config_value, config, sds_len(config));
    sds_delete(config);
    return result;
}

int config_load_file(config_manager_t* manager, char* filename) {
    return _config_load_file(manager, NULL, filename);
}

int _config_load_string(config_manager_t* manager, dict_t* config_value, char* str, size_t len) {
    char* err = NULL;
    int totlines = 0;
    sds_t* lines;
    lines = sds_split_len(str, len, "\n", 1, &totlines);
    int i;
    sds_t* argv = NULL;
    int argc = 0;
    for (i = 0; i < totlines; i++) {
        lines[i] = sds_trim(lines[i]," \t\r\n");
        sds_t line = lines[i];
        if (line[0] == '#' || line[0] == '\0') continue;
        
        argv = sds_split_args(line, &argc);
        if (argv == NULL) {
            err = "Unbalanced quotes in configuration line";
            goto error;
        }


        /* include file */
        if (str_cmp(argv[0], "include") == 0) {
            if (argc < 2) {
                err = "Include file name is missing";
                goto error;
            }
            if (_config_load_file(manager, config_value, argv[1]) == 0) {
                err = "Error loading include file";
                goto error;
            }
            sds_free_splitres(argv, argc);
            argv = NULL;
            argc = 0;
            continue;
        }

        /* load value */
        config_rule_t* rule_obj = config_get_rule(manager, argv[0]);
        if (rule_obj == NULL) {
            err = "Unknown configuration option";
            goto error;
        }
        void* value = rule_obj->load_value(rule_obj, argv + 1, argc - 1, &err);
        if (err != NULL) {
            goto error;
        }
        if (config_value != NULL) {
            dict_entry_t* entry = dict_find(config_value, argv[0]);
            if (entry == NULL) {
                dict_add(config_value, sds_dup(argv[0]), value);
            } else {
                if (!rule_obj->set_value(&dict_get_entry_val(entry), value)) {
                    err = "Error set value for configuration option";
                    goto error;
                }
            }
            
            
        } else {
            if (!rule_obj->set_value(rule_obj->data_ctx, value)) {
                err = "Error set value for configuration option";
                goto error;
            }
        }

        sds_free_splitres(argv, argc);
        argv = NULL;
        argc = 0;
    }
    sds_free_splitres(lines, totlines);
    
    return 1;
error:
    if (argv != NULL) sds_free_splitres(argv, argc);
    fprintf(stderr, "\n*** FATAL CONFIG  ERROR ***\n");
    fprintf(stderr, "Reading the configuration, at line %d\n", i + 1);
    fprintf(stderr, ">>> '%s'\n", lines[i]);
    fprintf(stderr, "%s\n", err);
    sds_free_splitres(lines, totlines);

    return 0;
}

int config_load_string(config_manager_t* manager, char* str, size_t len) {
    return _config_load_string(manager, NULL, str, len);
}

int config_load_argv(config_manager_t* manager,  char** argv, int argc) {
    sds format_conf_sds = sds_empty();
    int i = 0;
    while (i < argc) {
        if (argv[i][0] == '-' && argv[i][1] == '-') {
            /** Option name **/
            if (sds_len(format_conf_sds)) format_conf_sds = sds_cat(format_conf_sds, "\n");
            format_conf_sds = sds_cat(format_conf_sds, argv[i] + 2);
            format_conf_sds = sds_cat(format_conf_sds, " ");
        } else {
            /* maybe value is some params */
            /* Option argument*/
            format_conf_sds = sds_catrepr(format_conf_sds, argv[i], strlen(argv[i]));
            format_conf_sds = sds_cat(format_conf_sds, " ");
        }
        i++;
    }
    int ret = config_load_string(manager, format_conf_sds, sds_len(format_conf_sds));
    sds_delete(format_conf_sds);
    return ret;
}


dict_func_t config_dict_type_func = {
    .hashFunction = dict_char_hash,
    .keyCompare = dict_char_key_compare,
    .keyDestructor = dict_sds_destructor
};

sds config_diff_string(config_manager_t* manager, dict_t* old_config_dict);

int read_config_file_to_dict(config_manager_t* manager, dict_t* config_dict, char* filename) {
    latte_assert_with_info(config_dict != NULL, "config_dict is NULL");
    sds config = read_file_to_sds(filename);
    int ret = _config_load_string(manager, config_dict, config, sds_len(config));
    sds_delete(config);
    return ret;
}



dict_func_t sds_dict_type_func = {
    .hashFunction = dict_char_hash,
    .keyCompare = dict_char_key_compare,
    .keyDestructor = dict_sds_destructor,
    .valDestructor = dict_sds_destructor
};

sds config_diff_string(config_manager_t* manager, dict_t* old_config_dict) {
    sds result = NULL;
    dict_t* change_dict = dict_new(&sds_dict_type_func);
    latte_iterator_t* iter = dict_get_latte_iterator(manager->rules);
    while (latte_iterator_has_next(iter)) {
        latte_pair_t* pair = latte_iterator_next(iter);
        sds key = latte_pair_key(pair);
        config_rule_t* rule = latte_pair_value(pair);
        
        dict_entry_t* entry = dict_find(old_config_dict, key);
        sds now_str = rule->to_sds(rule, rule->get_value(rule->data_ctx));
        if (entry == NULL) {
            if (rule->default_value == NULL && now_str == NULL) {
                continue;
            }
            if (sds_cmp(now_str, rule->default_value) == 0) {
                sds_delete(now_str);
                continue;
            }   
        } else {
            void* old_value = dict_get_entry_val(entry);
            sds old_str= rule->to_sds(rule, old_value);  
            if (rule->delete_value != NULL) rule->delete_value(old_value);
            dict_set_val(old_config_dict, entry, NULL); 
            if (sds_cmp(old_str, now_str) == 0) {
                sds_delete(old_str);
                sds_delete(now_str);
                continue;
            }
            sds_delete(old_str);
        }
        dict_add(change_dict, sds_dup(key), now_str);  
        
    }
    latte_iterator_delete(iter);
    if (dict_size(change_dict) == 0) {
        goto end;
    }
    result = sds_new("\n# change config\n");
    iter = dict_get_latte_iterator(change_dict);
    while (latte_iterator_has_next(iter)) {
        latte_pair_t* pair = latte_iterator_next(iter);
        sds key = latte_pair_key(pair);
        sds value = latte_pair_value(pair);
        result = sds_cat(result, key);
        result = sds_cat(result, " ");
        result = sds_cat(result, value);
        result = sds_cat(result, "\n");
    }
    latte_iterator_delete(iter);
end:
    dict_delete(change_dict);
    return result;
}

sds config_diff_file(config_manager_t* manager, char* filename) {
    dict_t* old_config_dict = dict_new(&config_dict_type_func);
    latte_assert_with_info(read_config_file_to_dict(manager, old_config_dict ,filename) == 1, "error: read config file to dict failed");
    sds result = config_diff_string(manager, old_config_dict);
    dict_delete(old_config_dict);
    return result;
}

int config_save_file(config_manager_t* manager, char* filename) {
    sds config = config_diff_file(manager, filename);
    if (config == NULL) {
        return 1;
    }
    FILE *fp;
    if ((fp = fopen(filename, "a")) == NULL) {
        return 0;
    }
    fwrite(config, 1, sds_len(config), fp);
    fclose(fp);
    sds_delete(config);
    return 1;
}

/* 规则相关函数 */
/* 创建规则 */
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
    
) {
    config_rule_t* rule = zmalloc(sizeof(config_rule_t));
    rule->flags = flags;
    rule->data_ctx = data_ctx;
    rule->set_value = set_value;
    rule->get_value = get_value;
    rule->check_value = check_value;
    rule->to_sds = to_sds;
    rule->load_value = load_value;
    rule->cmp_value = cmp_value;
    rule->is_valid = is_valid;
    rule->default_value = default_value;
    rule->limit_arg = limit_arg;
    rule->delete_limit = delete_limit;
    rule->delete_value = delete_value;
    return rule;
}

/* 数值类型规则 */

int set_int64_value(void* data_ctx, void* value) {
    long long* data = (long long*)data_ctx;
    *data = (long long)(void*)value;
    return 1;
}

void* get_int64_value(void* data_ctx) {
    long long* data = (long long*)data_ctx;
    return (void*)*data;
}

void* load_int64_value(config_rule_t* rule, char** argv, int argc, char** error) {   
    UNUSED(rule);
    if (argc != 1) {
        *error = "error: argc != 1";
        return NULL;
    }
    sds value = (sds)argv[0];
    long long ll_value;
    int result = sds2ll(value, &ll_value);
    if (result == 0) {
        *error = "error: sds2ll failed";
        return NULL;
    }
    return (void*)ll_value;
}

int cmp_int64_value(config_rule_t* rule, void* a, void* b) {
    UNUSED(rule);
    return (long long)a - (long long)b;
}

int is_valid_int64_value(void* limit_arg, void* value) {
    long long ll_value = (long long)value;
    numeric_data_limit_t* limit = (numeric_data_limit_t*)(limit_arg);
    if (ll_value < limit->lower_bound || ll_value > limit->upper_bound) {
        return 0;
    }
    return 1;
}

sds to_sds_int64_value(config_rule_t* rule, void* data) {
    UNUSED(rule);
    return ll2sds((long long)data);
}
void numeric_limit_delete(void* limit_arg) {
    numeric_data_limit_t* limit = (numeric_data_limit_t*)(limit_arg);
    zfree(limit);
}

config_rule_t* config_rule_new_numeric_rule(int flags, long long* data_ctx, 
    long long lower_bound, long long upper_bound, check_value_func check_value,  long long default_value) {
    numeric_data_limit_t* limit = zmalloc(sizeof(numeric_data_limit_t));
    limit->lower_bound = lower_bound;
    limit->upper_bound = upper_bound;
    sds default_value_sds = ll2sds(default_value);
    config_rule_t* rule = config_rule_new(flags, 
        data_ctx, 
        set_int64_value, 
        get_int64_value, 
        check_value, 
        load_int64_value, 
        cmp_int64_value, 
        is_valid_int64_value, 
        to_sds_int64_value,
        limit,
        numeric_limit_delete,
        NULL,
        default_value_sds
    );
    return rule;
}

/* 字符串类型规则 */

int set_sds_value(void* data_ctx, void* value) {
    sds* data = (sds*)data_ctx;
    if (*data != NULL) {
        sds_delete(*data);
    }
    *data = (sds)value;
    return 1;
}
void* get_sds_value(void* data_ctx) {
    sds* data = (sds*)data_ctx;
    return (void*)*data;
}

void* load_sds_value(config_rule_t* rule,  char** argv, int argc, char** error) {  
    UNUSED(rule);
    if (argc != 1) {
        *error = "error: argc != 1";
        return NULL;
    }
    sds value = sds_dup((sds)argv[0]);
    return (void*)value;
}

int cmp_sds_value(config_rule_t* rule, void* a, void* b) {
    UNUSED(rule);
    return sds_cmp((sds)a, (sds)b);
}

int is_valid_sds_value(void* limit_arg, void* value) {
    // sds_data_limit_t* limit = (sds_data_limit_t*)(limit_arg);
    UNUSED(limit_arg);
    UNUSED(value);
    return 0;
}

sds to_sds_sds_value(config_rule_t* rule, void* data) {
    UNUSED(rule);
    if (data == NULL) {
        return NULL;
    }
    return sds_dup(data);
}

void config_sds_delete(void* data) {
    sds_delete((sds)data);
}

config_rule_t* config_rule_new_sds_rule(int flags, sds* data_ctx, 
    check_value_func check_value, sds default_value) {
    
    config_rule_t* rule = config_rule_new(flags, 
        data_ctx, 
        set_sds_value, 
        get_sds_value, 
        check_value, 
        load_sds_value, 
        cmp_sds_value, 
        is_valid_sds_value, 
        to_sds_sds_value, 
        NULL, 
        NULL,
        config_sds_delete,
        default_value
    );
    return rule;
}

/* 枚举类型规则 */

int set_enum_value(void* data_ctx, void* value) {
    int* data = (int*)data_ctx;
    *data = (int)(long long)value;
    return 1;
}

void* get_enum_value(void* data_ctx) {
    int* data = (int*)data_ctx;
    return (void*)(long long)*data;
}

void* load_enum_value(config_rule_t* rule, char** argv, int argc, char** error) {    
    if (argc != 1) {
        *error = "error: argc != 1";
        return NULL;
    }
    sds value = (sds)argv[0];
    enum_data_limit_t* limit = (enum_data_limit_t*)(rule->limit_arg);
    int i = 0;
    while (limit->enum_value[i].name != NULL) {
        if (str_cmp(value, limit->enum_value[i].name) == 0) {
            return (void*)(long long)limit->enum_value[i].val;
        }
        i++;
    }
    *error = "error: enum value not found";
    return NULL;
}

int cmp_enum_value(config_rule_t* rule, void* a, void* b) {
    UNUSED(rule);
    return (int)(long long)a - (int)(long long)b;
}

int is_valid_enum_value(void* limit_arg, void* value) {
    enum_data_limit_t* limit = (enum_data_limit_t*)(limit_arg);
    int i = 0;
    while (limit->enum_value[i].name != NULL) {
        if (str_cmp(value, limit->enum_value[i].name) == 0) {
            return 1;
        }
        i++;
    }
    return 0;
}

sds to_sds_enum_value(config_rule_t* rule, void* value) {
    int data = (int)(long long)value;
    enum_data_limit_t* limit = (enum_data_limit_t*)(rule->limit_arg);
    int i = 0;
    while (limit->enum_value[i].name != NULL) {
        if (limit->enum_value[i].val == data) {
            return sds_new(limit->enum_value[i].name);
        }
        i++;
    }
    latte_assert_with_info(0, "enum value not found");
    return NULL;
}
void enum_limit_delete(void* limit_arg) {
    enum_data_limit_t* limit = (enum_data_limit_t*)(limit_arg);
    zfree(limit);
}


config_rule_t* config_rule_new_enum_rule(int flags, void* data_ctx, 
    config_enum_t* enums, check_value_func check_value, sds default_value) {
    enum_data_limit_t* limit = zmalloc(sizeof(enum_data_limit_t));
    limit->enum_value = enums;
    config_rule_t* rule = config_rule_new(flags, 
        data_ctx, 
        set_enum_value, 
        get_enum_value, 
        check_value, 
        load_enum_value, 
        cmp_enum_value, 
        is_valid_enum_value, 
        to_sds_enum_value, 
        limit, 
        enum_limit_delete,
        NULL,
        default_value
    );
    return rule;
}

/* 布尔类型规则 */
int set_bool_value(void* data_ctx, void* value) {
    bool* data = (bool*)data_ctx;    
    *data = (bool)value;
    return 1;
}

void* get_bool_value(void* data_ctx) {
    bool* data = (bool*)data_ctx;
    return (void*)*data;   
}

void* load_bool_value(config_rule_t* rule, char** argv, int argc, char** error) {
    UNUSED(rule);
    if (argc != 1) {
        *error = "error: argc != 1";
        return NULL;
    }
    sds value = (sds)argv[0];
    if (str_cmp(value, "yes") == 0) {
        return (void*)true;
    } else if (str_cmp(value, "no") == 0) {
        return (void*)false;
    }
    *error = "error: bool value not found";
    return NULL;
}

int is_valid_bool_value(void* limit_arg, void* value) {
    UNUSED(limit_arg);
    if (str_cmp(value, "yes") == 0 || str_cmp(value, "no") == 0) {
        return 1;
    }
    return 0;
}

sds to_sds_bool_value(config_rule_t* rule, void* value) {
    UNUSED(rule);
    return (bool)value ? sds_new("yes") : sds_new("no");
}
int cmp_bool_value(config_rule_t* rule, void* a, void* b) {
    UNUSED(rule);
    return (bool)a == (bool)b ? 0 : 1;
}

config_rule_t* config_rule_new_bool_rule(int flags, bool* data_ctx, 
    check_value_func check_value, int default_value) {
    config_rule_t* rule = config_rule_new(flags, 
        data_ctx, 
        set_bool_value, 
        get_bool_value, 
        check_value, 
        load_bool_value, 
        cmp_bool_value, 
        is_valid_bool_value, 
        to_sds_bool_value, 
        NULL, 
        NULL,
        NULL,
        default_value ? sds_new("yes") : sds_new("no")
    );
    return rule;
}

/* 数组类型规则 */ 
void sds_array_delete(void* data) {
    vector_t* array = (vector_t*)data;
    while (vector_size(array) > 0) {
        sds_delete(vector_pop(array));
    }
    vector_delete(array);
} 
int set_sds_array_value(void* data_ctx, void* value) {
    vector_t* array = (vector_t*)value;
    vector_t** old_array = (vector_t**)data_ctx;
    if (*old_array != NULL) {
        sds_array_delete((void*)(*old_array));
    }
    *old_array = array;
    return 1;
}

void* get_sds_array_value(void* data_ctx) {
    return (void*)*((vector_t**)data_ctx);
}

void* load_sds_array_value(config_rule_t* rule, char** argv, int argc, char** error) {
    UNUSED(rule);
    UNUSED(error);
    
    vector_t* array = vector_new();
    for (int i = 0; i < argc; i++) {
        vector_push(array, sds_new(argv[i]));
    }
    return (void*)array;
}

int cmp_sds_array_value(config_rule_t* rule, void* a, void* b) {
    UNUSED(rule);
    vector_t* array_a = (vector_t*)a;
    vector_t* array_b = (vector_t*)b;
    if (vector_size(array_a) != vector_size(array_b)) {
        return 1;
    }
    for (size_t i = 0; i < vector_size(array_a); i++) {
        if (sds_cmp(vector_get(array_a, i) , vector_get(array_b, i)) != 0) {
            return 1;
        }
    }
    return 0;
}

int is_valid_sds_array_value(void* limit_arg, void* value) {
    array_data_limit_t* limit = (array_data_limit_t*)(limit_arg);
    vector_t* array = (vector_t*)value;
    if (limit->size != (size_t)-1 && vector_size(array) != limit->size) {
        return 0;
    }
    return 1;
}

sds to_sds_sds_array_value(config_rule_t* rule, void* data) {
    if (data == NULL) {
        return sds_new("");
    }
    UNUSED(rule);
    vector_t* array = (vector_t*)data;
    sds result = sds_empty();
    for (size_t i = 0; i < vector_size(array); i++) {
        result = sds_cat_fmt(result, "%s ", (sds)vector_get(array, i));
    }
    result[sds_len(result) - 1] = '\0';
    sds_set_len(result, sds_len(result) - 1);
    return result;
}

void array_limit_delete(void* limit_arg) {
    array_data_limit_t* limit = (array_data_limit_t*)(limit_arg);
    zfree(limit);
}



config_rule_t* config_rule_new_sds_array_rule(int flags, void* data_ctx, 
    check_value_func check_value, int size, sds default_value) {
    array_data_limit_t* limit = zmalloc(sizeof(array_data_limit_t));
    limit->size = size;
    config_rule_t* rule = config_rule_new(flags, 
        data_ctx, 
        set_sds_array_value, 
        get_sds_array_value, 
        check_value, 
        load_sds_array_value, 
        cmp_sds_array_value, 
        is_valid_sds_array_value, 
        to_sds_sds_array_value, 
        limit, 
        array_limit_delete,
        sds_array_delete,
        default_value
    );
    return rule;
}


/* map<sds, sds>类型规则  */
int set_map_sds_sds_value(void* data_ctx, void* value) {
    dict_t* map = (dict_t*)value;
    dict_t** old_map = (dict_t**)data_ctx;
    if (*old_map != NULL) {
        dict_delete(*old_map);
    }
    *old_map = map;
    return 1;
}
void* get_map_sds_sds_value(void* data_ctx) {
    return (void*)*((dict_t**)data_ctx);
}
dict_func_t map_sds_sds_dict_type_func = {
    .hashFunction = dict_char_hash,
    .keyCompare = dict_char_key_compare,
    .keyDestructor = dict_sds_destructor,
    .valDestructor = dict_sds_destructor,
};

void* load_map_sds_sds_value(config_rule_t* rule, char** argv, int argc, char** error) {   
    UNUSED(rule);
    if (argc % 2 != 0) {
        *error = "error: argc % 2 != 0";
        return NULL;
    }
    dict_t* map = dict_new(&map_sds_sds_dict_type_func);
    for (int i = 0; i < argc; i+=2) {
        sds key = sds_new(argv[i]);
        sds value = sds_new(argv[i+1]);
        if (dict_add(map, key, value)) {
            *error = "error: dict_add failed";
            dict_delete(map);
            sds_delete(key);
            sds_delete(value);
            return NULL;
        }
    }
    return (void*)map;
}

int cmp_map_sds_sds_value(config_rule_t* rule, void* a, void* b) {
    UNUSED(rule);
    dict_t* map_a = (dict_t*)a;
    dict_t* map_b = (dict_t*)b;
    if (dict_size(map_a) != dict_size(map_b)) {
        return 1;
    }
    latte_iterator_t* iter = dict_get_latte_iterator(map_a);
    while (latte_iterator_has_next(iter)) {
        latte_pair_t* pair = latte_iterator_next(iter);
        sds key = pair->key;
        sds value = pair->value;
        sds b_value = dict_fetch_value(map_b, key); 
        if (b_value == NULL) {
            return 1;
        }
        if (sds_cmp(value, b_value) != 0) {
            return 1;
        }
    }
    latte_iterator_delete(iter);
    return 0;
}

typedef struct map_sds_sds_data_limit_t {
    char** keys;
} map_sds_sds_data_limit_t;

int is_valid_map_sds_sds_value(void* limit_arg, void* value) {
    int result = 1;
    if (limit_arg == NULL) {
        return 1;
    }
    map_sds_sds_data_limit_t* limit = (map_sds_sds_data_limit_t*)(limit_arg);
    dict_t* map = (dict_t*)value;
    latte_iterator_t* iter = dict_get_latte_iterator(map);
    while (latte_iterator_has_next(iter)) {
        latte_pair_t* pair = latte_iterator_next(iter);
        sds key = pair->key;
        int finded = 0; 
        for (int i = 0; limit->keys[i] != NULL; i++) {
            if (str_cmp(key, limit->keys[i]) == 0) {
                finded = 1;
                break;
            }
        }
        if (!finded) {
            result = 0;
            break;
        }
    }
    latte_iterator_delete(iter);
    return result;
}

sds to_sds_map_sds_sds_value(config_rule_t* rule, void* data) {
    if (data == NULL) {
        return sds_new("");
    }
    UNUSED(rule);
    latte_iterator_t* iter = dict_get_latte_iterator(data);
    sds result = sds_empty_len(512);
    while (latte_iterator_has_next(iter)) {
        latte_pair_t* pair = latte_iterator_next(iter);
        sds key = pair->key;
        sds value = pair->value;
        result = sds_cat_fmt(result, "%s %s ", key, value);
    }
    latte_iterator_delete(iter);
    result[sds_len(result) - 1] = '\0';
    sds_set_len(result, sds_len(result) - 1);
    return result;
}

void map_sds_sds_limit_delete(void* limit_arg) {
    map_sds_sds_data_limit_t* limit = (map_sds_sds_data_limit_t*)(limit_arg);
    zfree(limit);
}

void config_sds_map_delete(void* data) {
    dict_t* map = (dict_t*)data;
    dict_delete(map);
}

config_rule_t* config_rule_new_map_sds_sds_rule(int flags, void* data_ctx, 
    check_value_func check_value, char** keys, sds default_value) {
    map_sds_sds_data_limit_t* limit = zmalloc(sizeof(map_sds_sds_data_limit_t));
    limit->keys = keys;
    config_rule_t* rule = config_rule_new(flags, 
        data_ctx, 
        set_map_sds_sds_value, 
        get_map_sds_sds_value, 
        check_value, 
        load_map_sds_sds_value, 
        cmp_map_sds_sds_value, 
        is_valid_map_sds_sds_value, 
        to_sds_map_sds_sds_value, 
        limit, 
        map_sds_sds_limit_delete,
        config_sds_map_delete,
        default_value
    );
    return rule;
}

/* 多段map<sds,sds>属性插入规则 */   
int append_map_sds_sds_value(void* data_ctx, void* value) {
    dict_t* map = (dict_t*)value;
    dict_t* old_map = *(dict_t**)data_ctx;
    if (old_map == NULL) {
        old_map = map;
    } else {
        // dict_join(old_map, map);
        latte_iterator_t* iter = dict_get_latte_iterator(map);
        while (latte_iterator_has_next(iter)) {
            latte_pair_t* pair = latte_iterator_next(iter);
            sds key = pair->key;
            sds value = pair->value;
            dict_entry_t* entry = dict_find(old_map, key);
            if (entry == NULL) {
                dict_add(old_map, sds_dup(key), sds_dup(value));
            } else {
                latte_assert_with_info(old_map->type->valDestructor != NULL, "valDestructor is NULL");
                dict_free_val(old_map, entry);
                dict_set_val(old_map,entry, sds_dup(value));
            }
        }
        latte_iterator_delete(iter);
        dict_delete(map);
    }
    *(dict_t**)data_ctx = old_map;
    return 1;
}

int cmp_append_map_sds_sds_value(config_rule_t* rule, void* a, void* b) {
    UNUSED(rule);
    dict_t* map_a = (dict_t*)a;
    dict_t* map_b = (dict_t*)b;
    latte_iterator_t* iter = dict_get_latte_iterator(map_b);
    int result = 0;
    while (latte_iterator_has_next(iter)) {
        latte_pair_t* pair = latte_iterator_next(iter);
        sds key = pair->key;
        sds value_b = pair->value;
        sds value_a =  dict_fetch_value(map_a, key);
        if (value_a == NULL || sds_cmp(value_b, value_a) != 0) {
            result = 1;
            break;  
        }
    }
    latte_iterator_delete(iter);
    return result;
}

config_rule_t* config_rule_new_append_map_sds_sds_rule(int flags, void* data_ctx, 
    check_value_func check_value, char** keys, sds default_value) {
    map_sds_sds_data_limit_t* limit = zmalloc(sizeof(map_sds_sds_data_limit_t));
    limit->keys = keys;
    config_rule_t* rule = config_rule_new(flags, 
        data_ctx, 
        append_map_sds_sds_value, 
        get_map_sds_sds_value, 
        check_value, 
        load_map_sds_sds_value, 
        cmp_append_map_sds_sds_value, 
        is_valid_map_sds_sds_value, 
        to_sds_map_sds_sds_value, 
        limit, 
        map_sds_sds_limit_delete,
        config_sds_map_delete,
        default_value
    );
    return rule;
}

/* 删除规则 */
void config_rule_delete(config_rule_t* rule) {
    if (rule->delete_limit != NULL) rule->delete_limit(rule->limit_arg);
    if (rule->default_value != NULL) sds_delete(rule->default_value);
    zfree(rule);
}

/* 设置值 */
int config_set_value(config_manager_t* manager, char** argv, int argc, char** err) {
    config_rule_t* rule = config_get_rule(manager, argv[0]);
    if (rule == NULL) {
        *err = "Error: rule not found";
        return 0;
    }
    void * value = rule->load_value(rule, argv + 1, argc - 1, err);
    if (*err != NULL) {
        return 0;
    }
    if (rule->check_value != NULL) {
        if (!rule->check_value(rule, rule->get_value(rule->data_ctx), value)) {
            *err = "Error: check_value failed";
            if (rule->delete_value != NULL) {
                rule->delete_value(value);
            }
            return 0;
        }
    }

    if (rule->cmp_value(rule, rule->get_value(rule->data_ctx), value) == 0) {
        if (rule->delete_value != NULL) {
            rule->delete_value(value);
        }
        return 0;
    }
    return rule->set_value(rule->data_ctx, value);
}
/* 获取值 */
void* config_get_value(config_manager_t* manager, char* key) {
    config_rule_t* rule = config_get_rule(manager, key);
    if (rule == NULL) {
        LATTE_LIB_LOG(LOG_ERROR, "Error: %s rule not found", key);
        return NULL;
    }
    return rule->get_value(rule->data_ctx);
}

sds config_rule_to_sds(config_manager_t* manager, char* key) {
    config_rule_t* rule = config_get_rule(manager, key);
    if (rule == NULL) {
        LATTE_LIB_LOG(LOG_ERROR, "Error: %s rule not found", key);
        return NULL;
    }
    sds value_str = rule->to_sds(rule, rule->get_value(rule->data_ctx));
    sds result = sds_cat_fmt(sds_new(key), " %s", value_str);
    sds_delete(value_str);
    return result;
}