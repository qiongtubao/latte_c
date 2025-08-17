

#include "test/testhelp.h"
#include "test/testassert.h"

#include <stdio.h>
#include "config.h"
#include "zmalloc/zmalloc.h"
#include <sys/time.h>
#include "log/log.h"
#include <stdbool.h>


/* sds_rule test*/
int test_config_sds_rule(void) {
    config_manager_t* manager = config_manager_new();
    sds name = NULL;
    /*name */
    config_rule_t* rule = config_rule_new_sds_rule(0, &name, NULL, sds_new("test"));
    config_add_rule(manager, "name", rule);
    char* configstr = "name latte1";
    assert(config_load_string(manager, configstr, strlen(configstr)) == 1);
    assert(sds_len(name) == 6);
    assert(strcmp(name, "latte1") == 0);
    sds_delete(name);
    config_manager_delete(manager);
    return 1;
}

/* numeric_rule test*/
int test_config_numeric_rule(void) {
    config_manager_t* manager = config_manager_new();
    int64_t age = 0;
    config_rule_t* rule = config_rule_new_numeric_rule(0, &age, 0, 100, NULL, 0);
    config_add_rule(manager, "age", rule);
    char* configstr = "age 10";
    assert(config_load_string(manager, configstr, strlen(configstr)) == 1);
    assert(age == 10);
    configstr = "age error";
    assert(config_load_string(manager, configstr, strlen(configstr)) == 0);
    config_manager_delete(manager);
    return 1;
}

/* enum_rule test*/
typedef enum gender_enum{
    MAN,
    WOMAN,
} gender_enum;

config_enum_t gender_enum_list[] = {
    { "man", MAN},
    { "woman", WOMAN},
    { NULL,  0},
};

int test_config_enum_rule(void) {
    config_manager_t* manager = config_manager_new();
    gender_enum gender = MAN;
    config_rule_t* rule = config_rule_new_enum_rule(0, &gender, gender_enum_list,NULL, sds_new("man"));
    config_add_rule(manager, "gender", rule);
    char* configstr = "gender woman";
    assert(config_load_string(manager, configstr, strlen(configstr)) == 1);
    assert(gender == WOMAN);
    configstr = "gender error";
    assert(config_load_string(manager, configstr, strlen(configstr)) == 0);
    config_manager_delete(manager);
    return 1;
}

/* 布尔规则测试*/
int test_config_bool_rule(void) {
    config_manager_t* manager = config_manager_new();
    bool is_man = false;
    config_rule_t* rule = config_rule_new_bool_rule(0, &is_man, NULL, true);
    config_add_rule(manager, "is_man", rule);
    char* configstr = "is_man no";
    assert(config_load_string(manager, configstr, strlen(configstr)) == 1);
    assert(is_man == false);
    configstr = "is_man error";
    assert(config_load_string(manager, configstr, strlen(configstr)) == 0);
    config_manager_delete(manager);
    return 1;
}

/* 数组规则测试 */
int test_config_array_rule(void) {
    config_manager_t* manager = config_manager_new();
    vector_t* likes = NULL;
    config_rule_t* rule = config_rule_new_sds_array_rule(0, &likes, NULL, -1, sds_new(""));
    config_add_rule(manager, "likes", rule);
    char* configstr = "likes a b c";
    assert(config_load_string(manager, configstr, strlen(configstr)) == 1);
    assert(vector_size(likes) == 3);
    assert(strcmp(vector_get(likes, 0), "a") == 0);
    assert(strcmp(vector_get(likes, 1), "b") == 0);
    assert(strcmp(vector_get(likes, 2), "c") == 0);
    sds value = NULL;
    while (vector_size(likes) > 0) {
        value = vector_pop(likes);
        sds_delete(value);
    }
    vector_delete(likes);
    config_manager_delete(manager);
    return 1;
}

int test_api(void) {
    log_module_init();
    assert(log_add_stdout(LATTE_LIB, LOG_DEBUG) == 1);
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("config_rule test sds_rule function",
            test_config_sds_rule() == 1);
        test_cond("config_rule test numeric_rule function",
            test_config_numeric_rule() == 1);
        test_cond("config_rule test enum_rule function",
            test_config_enum_rule() == 1);
        test_cond("config_rule test bool_rule function",
            test_config_bool_rule() == 1);
        test_cond("config_rule test array_rule function",
            test_config_array_rule() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}