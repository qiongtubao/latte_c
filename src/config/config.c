#include "config.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "utils/utils.h"

#include "dict/dict_plugins.h"


#define UNUSED(x) ((void)(x))
// void* ll2ptr(long long value) {
//     if (sizeof(void*) == sizeof(long long)) {
//         return (void*)(value);
//     } else {
//         return (void*)&value;
//     }
// }


long long ptr2ll(void* value) {
    if (sizeof(void*) == sizeof(long long)) {
        return *(long long*)(&value);
    } else {
        return *(long long *)value;
    }
}




dict_func_t ruleDictType = {
    dict_char_hash,
    NULL,
    NULL,
    dict_char_key_compare,
    dict_sds_destructor,
    NULL,
    NULL
};

struct config_manager_t* config_manager_new() {
    config_manager_t* c = zmalloc(sizeof(config_manager_t));
    c->rules = dict_new(&ruleDictType);
    return c;
}

void config_manager_delete(config_manager_t* c) {
    latte_iterator_t* di = dict_get_latte_iterator(c->rules);
    while (latte_iterator_has_next(di)) {
        config_rule_t* rule = (config_rule_t*)latte_pair_value((latte_iterator_next(di)));
        value_clean(&rule->value);
    }
    latte_iterator_delete(di);
    dict_delete(c->rules);
    zfree(c);
}

config_rule_t* config_get_rule(struct config_manager_t* c, char* key) {
    dict_entry_t* entry = dict_find(c->rules, key);
    if (entry == NULL) return NULL;
    return dict_get_val(entry);
}

value_t* config_get(config_manager_t* c, char* key) {
    config_rule_t* rule = config_get_rule(c, key);
    if (rule == NULL) return NULL;
    return &rule->value;
}

sds_t config_get_sds(config_manager_t* c, char* key) {
    return value_get_sds(config_get(c, key));
}

int64_t config_get_int64(config_manager_t* c, char* key) {
    return value_get_int64(config_get(c, key));
}

vector_t* config_get_array(config_manager_t* c, char* key) {
    return value_get_array(config_get(c, key));
}

int config_set_sds(config_manager_t* c, char* key, sds_t value) {
    config_rule_t* rule = config_get_rule(c, key);
    if (rule == NULL) return 0;
    value_t* v = &rule->value;
    latte_assert(value_is_null(v) || value_is_sds(v), "config set fail: type is not sds ");
    if (value_is_sds(v) && sds_cmp(value_get_sds(v),value) == 0) return 0;
    if (rule->check_update == NULL || rule->check_update(rule, (void*)value_get_sds(v), (void*)value)) {
        value_set_sds(v, value);
        return 1;
    }
    return 0;
}

int config_set_int64(config_manager_t* c, char* key, int64_t value) {
    config_rule_t* rule = config_get_rule(c, key);
    if (rule == NULL) return 0;
    value_t* v = &rule->value;
    latte_assert(value_is_null(v) || value_is_int64(v), "config set fail: type is not int64 ");
    if (value_get_int64(v) == value) return 0;
    if (rule->check_update == NULL  || rule->check_update(rule, (void*)value_get_int64(v), (void*)value)) {
        value_set_int64(v, value);
        return 1;
    }
    return 0;
}


int config_set_array(config_manager_t* c, char* key, vector_t* value) {
    config_rule_t* rule = config_get_rule(c, key);
    if (rule == NULL) return 0;
    value_t* v = &rule->value;
    latte_assert(value_is_null(v) || value_is_array(v), "config set fail: type is not array ");
    if (private_vector_cmp(value_get_array(v) ,value, value_cmp) == 0) return 0;
    if (rule->check_update == NULL  || rule->check_update(rule, (void*)value_get_array(v), (void*)value)) {
        value_set_array(v, value);
        return 1;
    }
    return 0;
}

// int configSetLongLong(config* c, char* key, long long value) {
//     dict_entry_t* entry = dict_find(c->rules, key);
//     if (entry == NULL) return 0;
//     config_rule_t* rule = dict_get_val(entry);
//     if (rule->update(rule, rule->value, ll2ptr(value))) {
//         rule->value = *(void **)(&value);
//     }
//     return 1;
// }

int config_register_rule(config_manager_t* c, sds_t key, config_rule_t* rule) {
    
    if(rule->value.type == VALUE_CONSTANT_CHAR) {
        int len = 0;
        sds* result = sds_split_args(rule->value.value.sds_value, &len);
        rule->load(rule, result, len);
        sds_free_splitres(result, len);
        // printf("hello %s\n", rule->value.value.sds_value);
        // value_set_sds(&rule->value, sds_new(rule->value.value.sds_value));
    }
    return dict_add(c->rules, key, rule);
}


int load_config_from_string(config_manager_t* config, char* configstr, size_t configstrlen) {
    const char *err = NULL;
    int linenum = 0, totlines, i;
    sds_t *lines;
    lines = sds_split_len(configstr, configstrlen, "\n",1,&totlines);
    for (i = 0; i < totlines; i++) {
        sds_t *argv;
        int argc;

        linenum = i+1;
        lines[i] = sds_trim(lines[i]," \t\r\n");
        /* Skip comments and blank lines */
        if (lines[i][0] == '#' || lines[i][0] == '\0') continue;

        /* Split into arguments */
        argv = sds_split_args(lines[i],&argc);
        if (argv == NULL) {
            err = "Unbalanced quotes in configuration line";
            sds_free_splitres(argv, argc);
            goto loaderr;
        }
        // sdstolower(argv[0]);
        config_rule_t* rule = config_get_rule(config, argv[0]);
        if (rule == NULL) {
            err = "Bad directive or wrong number of arguments";
            sds_free_splitres(argv, argc);
            goto loaderr;
        }
        if(!config_set_argv(rule, argv+1, argc-1)) {
            sds_free_splitres(argv, argc);
            err = "load config wrong";
            goto loaderr;
        }
        sds_free_splitres(argv, argc);
    }
    return 1;
loaderr:
    fprintf(stderr, "\n*** FATAL CONFIG  ERROR ***\n");
    fprintf(stderr, "Reading the configuration, at line %d\n", linenum);
    fprintf(stderr, ">>> '%s'\n", lines[i]);
    fprintf(stderr, "%s\n", err);
    return 0;
}

int config_set_argv(config_rule_t* rule, char** argv, int argc) {
    return rule->load(rule, argv, argc);
}

int load_config_from_argv(config_manager_t* c, char** argv, int argc) {
    sds_t format_conf_sds = sds_empty();
    int i = 0;
    while(i < argc) {
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
    int result = load_config_from_string(c, format_conf_sds, sds_len(format_conf_sds));
    sds_delete(format_conf_sds);
    return result;
}
#define CONFIG_MAX_LINE 1024
int load_config_from_file(config_manager_t* c, char *filename) {
    sds_t config = sds_empty();
    char buf[CONFIG_MAX_LINE+1];
    FILE *fp;

    /* Load the file content */
    if (filename) {
        if ((fp = fopen(filename,"r")) == NULL) {
            printf(
                    "Fatal error, can't open config file '%s': %s\n",
                    filename, strerror(errno));
            exit(1);
        }
        while(fgets(buf,CONFIG_MAX_LINE+1,fp) != NULL)
            config = sds_cat(config,buf);
        fclose(fp);
    }
    int result = load_config_from_string(c, config, sds_len(config));
    sds_delete(config);
    return result;
}


/* sds_t config gen*/

sds_t write_config_sds(sds_t config, char* key, config_rule_t* rule) {
    value_t* v = &rule->value;
    latte_assert(value_is_sds(v), "write_config_sds :value is not sds");
    return sds_cat_printf(config, "%s %s", key, value_get_sds(v));
}

sds_t write_config_sds_array(sds_t config, char* key, config_rule_t* rule) {
    value_t* v = &rule->value;
    latte_assert(value_is_array(v), "write_config_array :value is not array");
    vector_t* array = value_get_array(v);
    latte_iterator_t* it = vector_get_iterator(array);
    sds result = sds_empty_len(100);
    int is_first = 1;
    while(latte_iterator_has_next(it)) {
        value_t* node = latte_iterator_next(it);
        if (is_first) {
            result = sds_cat_fmt(result, "%s", value_get_sds(node));
            is_first = 0;
        } else {
            result = sds_cat_fmt(result, " %s", value_get_sds(node));
        }
    }
    latte_iterator_delete(it);
    return sds_cat_printf(config, "%s %s", key, result);
}

void value_delete_sds(void* value) {
    if(value) {
        sds_delete(value);
    }
}
int load_config_sds(config_rule_t* rule, char** argv, int argc) {
    if (argc != 1) return 0;
    value_set_sds(&rule->value,sds_new_len(argv[0], strlen(argv[0])));
    return 1;
}

/* num config gen*/

sds_t write_config_int64(sds_t config, char* key, config_rule_t* rule) {
    int64_t ll = value_get_int64(&rule->value);
    return sds_cat_printf(config, "%s %lld", key, ll);
}


void value_delete_int64(void* value) {
    UNUSED(value);
}

int load_config_int64(config_rule_t* rule, char** argv, int argc) {
    if (argc != 1) return 0;
    long long ll = 0;
    if (!string2ll(argv[0], strlen(argv[0]), &ll)) {
        return 0;
    }
    // rule->value = (void*)ll;
    value_set_int64(&rule->value, ll);
    return 1;
}



// struct arraySds* configGetArray(config* c, char* key) {
//     return ((struct arraySds*)(configGet(c, key)));
// }

// /* sds_t config gen*/
// int arraySdsUpdate(config_rule_t* rule, void* old_value, void* new_value) {
//     rule->value = new_value;
//     return 1;
// }

// sds_t arraySdsWriteConfigSds(sds_t config, char* key, config_rule_t* rule) {
//     struct arraySds* value = (struct arraySds*)rule->value;
//     sds_t result = sds_cat_printf(config, "%s", key);
//     for(int i = 0; i < value->len; i++) {
//         result = sds_cat_printf(config, " %s", value->value[i]);
//     }
//     return result;
// }

// void arraySdsReleaseValue(void* _value) {
//     if (_value == NULL) return;
//     struct arraySds* value = (struct arraySds*)_value;
//     for(int i = 0; i < value->len; i++) {
//         sds_delete(value->value[i]);
//     }
//     zfree(value->value);
//     zfree(value);
// }
int load_config_sds_array(config_rule_t* rule, char** argv, int argc) {
    if (argc < 1) return 0;
    vector_t* result = vector_new();
    for(int i = 0; i < argc; i++) {
        value_t* node = value_new();
        value_set_sds(node, sds_new(argv[i]));
        vector_push(result, node);
    }
    value_set_array(&rule->value, result);
    return 1;
}
// int arraySdsLoadConfig(config_rule_t* rule, char** argv, int argc) {
//     if (argc < 2) return 0;
//     sds* array = zmalloc(sizeof(sds*) * (argc - 1));
//     struct arraySds* value = zmalloc(sizeof(struct arraySds));
//     for(int i = 1; i < argc; i++) {
//         array[i - 1] = sds_new(argv[i]);
//     }
//     value->len = (argc - 1);
//     value->value = array;
//     rule->value = value;
//     return 1;
// }

sds_t write_config_to_sds(config_manager_t* c) {
    latte_iterator_t* it = dict_get_latte_iterator(c->rules);
    int first = 1;
    sds result = sds_empty_len(100);
    while(latte_iterator_has_next(it)) {
        latte_pair_t* pair = latte_iterator_next(it);
        sds key = latte_pair_key(pair);
        config_rule_t* rule = latte_pair_value(pair);
        if (first) {
            first = 0;
        } else {
            result = sds_cat(result, "\r\n");
        }
        result = rule->to_sds(result, key, rule);
    }
    latte_iterator_delete(it);
    return result;
}
