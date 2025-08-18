

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
    assert(config_add_rule(manager, "name", rule) == 0);
    assert(config_init_all_data(manager) == 1);
    assert(sds_len(name) == 4);
    assert(strcmp(name, "test") == 0);

    char* configstr = "name latte1";
    assert(config_load_string(manager, configstr, strlen(configstr)) == 1);
    assert(sds_len(name) == 6);
    assert(strcmp(name, "latte1") == 0);
    config_rule_t* rule2 = config_get_rule(manager, "name");
    assert(rule2 == rule);
    char* err = NULL;
    char* argv[] = {sds_new("name"), sds_new("latte2")};
    assert(config_set_value(manager, argv, 2, &err) == 1);
    assert(err == NULL);
    assert(sds_len(name) == 6);
    assert(strcmp(name, "latte2") == 0);


    void* value = config_get_value(manager, "name");
    assert(value == name);

    sds sds_str = config_rule_to_sds(manager, "name");
    assert(sds_len(sds_str) == 11);
    assert(strcmp(sds_str, "name latte2") == 0);
    sds_delete(sds_str);


    sds_delete(argv[0]);
    sds_delete(argv[1]);

    sds_delete(name);
    config_manager_delete(manager);
    return 1;
}

/* numeric_rule test*/
int test_config_numeric_rule(void) {
    config_manager_t* manager = config_manager_new();
    int64_t age = 0;
    config_rule_t* rule = config_rule_new_numeric_rule(0, &age, 0, 100, NULL, 1);
    config_add_rule(manager, "age", rule);
    assert(config_init_all_data(manager) == 1);
    assert(age == 1);
    char* configstr = "age 10";
    assert(config_load_string(manager, configstr, strlen(configstr)) == 1);
    assert(age == 10);
    configstr = "age error";
    assert(config_load_string(manager, configstr, strlen(configstr)) == 0);


    char* err = NULL;
    char* argv[] = {sds_new("age"), sds_new("11")};
    assert(config_set_value(manager, argv, 2, &err) == 1);
    assert(err == NULL);
    assert(age == 11);

    void* value = config_get_value(manager, "age");
    assert(value == age);
    assert(value == 11);

    sds sds_str = config_rule_to_sds(manager, "age");
    assert(sds_len(sds_str) == 6);
    assert(strcmp(sds_str, "age 11") == 0);
    sds_delete(sds_str);

    sds_delete(argv[0]);
    sds_delete(argv[1]);
    
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

    assert(config_init_all_data(manager) == 1);
    assert(gender == MAN);

    char* configstr = "gender woman";
    assert(config_load_string(manager, configstr, strlen(configstr)) == 1);
    assert(gender == WOMAN);
    configstr = "gender error";
    assert(config_load_string(manager, configstr, strlen(configstr)) == 0);

    char* err = NULL;
    char* argv[] = {sds_new("gender"), sds_new("man")};
    assert(config_set_value(manager, argv, 2, &err) == 1);
    assert(err == NULL);
    assert(gender == MAN);

    void* value = config_get_value(manager, "gender");
    assert(value == gender);
    assert(value == MAN);

    sds sds_str = config_rule_to_sds(manager, "gender");
    assert(sds_len(sds_str) == 10);
    assert(strcmp(sds_str, "gender man") == 0);
    sds_delete(sds_str);

    sds_delete(argv[0]);
    sds_delete(argv[1]);
    config_manager_delete(manager);
    return 1;
}

/* 布尔规则测试*/
int test_config_bool_rule(void) {
    config_manager_t* manager = config_manager_new();
    bool is_man = false;
    config_rule_t* rule = config_rule_new_bool_rule(0, &is_man, NULL, true);
    config_add_rule(manager, "is_man", rule);

    assert(config_init_all_data(manager) == 1);
    assert(is_man == true);
    char* configstr = "is_man no";
    assert(config_load_string(manager, configstr, strlen(configstr)) == 1);
    assert(is_man == false);
    configstr = "is_man error";
    assert(config_load_string(manager, configstr, strlen(configstr)) == 0);

    char* err = NULL;
    char* argv[] = {sds_new("is_man"), sds_new("yes")};
    assert(config_set_value(manager, argv, 2, &err) == 1);
    assert(err == NULL);
    assert(is_man == true);

    void* value = config_get_value(manager, "is_man");
    assert(value == is_man);
    assert(value == true);

    sds sds_str = config_rule_to_sds(manager, "is_man");
    assert(sds_len(sds_str) == 10);
    assert(strcmp(sds_str, "is_man yes") == 0);
    sds_delete(sds_str);

    sds_delete(argv[0]);
    sds_delete(argv[1]);

    config_manager_delete(manager);
    return 1;
}

/* 数组规则测试 */
int test_config_array_rule(void) {
    config_manager_t* manager = config_manager_new();
    vector_t* likes = NULL;
    config_rule_t* rule = config_rule_new_sds_array_rule(0, &likes, NULL, -1, sds_new(""));
    config_add_rule(manager, "likes", rule);
    assert(config_init_all_data(manager) == 1);
    assert(likes != NULL);
    assert(vector_size(likes) == 0);
    char* configstr = "likes a b c";
    assert(config_load_string(manager, configstr, strlen(configstr)) == 1);
    assert(vector_size(likes) == 3);
    assert(strcmp(vector_get(likes, 0), "a") == 0);
    assert(strcmp(vector_get(likes, 1), "b") == 0);
    assert(strcmp(vector_get(likes, 2), "c") == 0);

    char* err = NULL;
    char* argv[] = {sds_new("likes"), sds_new("d")};
    assert(config_set_value(manager, argv, 2, &err) == 1);
    assert(err == NULL);
    assert(vector_size(likes) == 1);
    assert(strcmp(vector_get(likes, 0), "d") == 0);

    void* value = config_get_value(manager, "likes");
    assert(value == likes);

    sds sds_str = config_rule_to_sds(manager, "likes");
    assert(sds_len(sds_str) == 7);
    assert(strcmp(sds_str, "likes d") == 0);
    sds_delete(sds_str);


    sds_delete(argv[0]);
    sds_delete(argv[1]);


    while (vector_size(likes) > 0) {
        sds_delete( vector_pop(likes));
    }
    vector_delete(likes);
    config_manager_delete(manager);
    return 1;
}

/* map_sds_sds规则测试 */
int test_config_map_sds_sds_rule(void) {
    config_manager_t* manager = config_manager_new();
    dict_t* map = NULL;
    config_rule_t* rule = config_rule_new_map_sds_sds_rule(0, &map, NULL, NULL, sds_new(""));
    config_add_rule(manager, "map", rule);
    assert(config_init_all_data(manager) == 1);
    assert(map != NULL);
    assert(dict_size(map) == 0);
    char* configstr = "map k1 v1 k2 v2";
    assert(config_load_string(manager, configstr, strlen(configstr)) == 1);
    assert(dict_size(map) == 2);
    assert(strcmp(dict_fetch_value(map, "k1"), "v1") == 0);
    assert(strcmp(dict_fetch_value(map, "k2"), "v2") == 0);

    char* err = NULL;
    char* argv[] = {sds_new("map"), sds_new("k3"), sds_new("v3")};
    assert(config_set_value(manager, argv, 3, &err) == 1);
    assert(err == NULL);
    assert(dict_size(map) == 1);
    assert(strcmp(dict_fetch_value(map, "k3"), "v3") == 0);

    void* value = config_get_value(manager, "map");
    assert(value == map);

    sds_delete(argv[0]);
    sds_delete(argv[1]);
    sds_delete(argv[2]);
    dict_delete(map);
    config_manager_delete(manager);
    return 1;
}

/* map_append_sds_sds规则测试 */
int test_config_map_append_sds_sds_rule(void) {
    config_manager_t* manager = config_manager_new();
    dict_t* map = NULL;
    config_rule_t* rule = config_rule_new_append_map_sds_sds_rule(0, &map, NULL, NULL, sds_new(""));
    config_add_rule(manager, "map", rule);
    assert(config_init_all_data(manager) == 1);
    assert(map != NULL);
    assert(dict_size(map) == 0);
    char* configstr = "map k1 v1";
    assert(config_load_string(manager, configstr, strlen(configstr)) == 1);
    assert(dict_size(map) == 1);
    assert(strcmp(dict_fetch_value(map, "k1"), "v1") == 0);
    configstr = "map k2 v2";
    assert(config_load_string(manager, configstr, strlen(configstr)) == 1);
    assert(dict_size(map) == 2);
    assert(strcmp(dict_fetch_value(map, "k2"), "v2") == 0);

    /* add k3 v3 */
    char* err = NULL;
    char* argv[] = {sds_new("map"), sds_new("k3"), sds_new("v3")};
    assert(config_set_value(manager, argv, 3, &err) == 1);
    assert(err == NULL);
    assert(dict_size(map) == 3);
    assert(strcmp(dict_fetch_value(map, "k3"), "v3") == 0);

    /* set k1 v1 */
    sds_delete(argv[1]);
    sds_delete(argv[2]);
    argv[1] = sds_new("k1");
    argv[2] = sds_new("v2");
    assert(config_set_value(manager, argv, 3, &err) == 1);
    assert(err == NULL);
    assert(dict_size(map) == 3);
    assert(strcmp(dict_fetch_value(map, "k1"), "v2") == 0);

    sds sds_str = config_rule_to_sds(manager, "map");
    LATTE_LIB_LOG(LL_INFO, "sds_str : %s", sds_str);
    assert(sds_len(sds_str) == 21);
    assert(strcmp(sds_str, "map k1 v2 k2 v2 k3 v3") == 0);
    sds_delete(sds_str);

    sds_delete(argv[0]);
    sds_delete(argv[1]);
    sds_delete(argv[2]);
    dict_delete(map);
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
        test_cond("config_rule test map_sds_sds_rule function",
            test_config_map_sds_sds_rule() == 1);
        test_cond("config_rule test map_append_sds_sds_rule function",
            test_config_map_append_sds_sds_rule() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}