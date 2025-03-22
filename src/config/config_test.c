#include "test/testhelp.h"
#include "test/testassert.h"

#include <stdio.h>
#include "config.h"
#include "zmalloc/zmalloc.h"
#include <sys/time.h>



config_rule_t name_rule = SDS_CONFIG_INIT("test_name");
#define DEFAULT_AGE (10)
config_rule_t age_rule = LL_CONFIG_INIT(DEFAULT_AGE);
config_rule_t like_rule = SDS_ARRAY_CONFIG_INIT("跑步 旅行");

int test_configCreate() {
    config_manager_t* c = config_manager_new();
    //sds
    sds_t name = sds_new("name");
    config_register_rule(c, name, &name_rule);
    //long long
    sds_t age = sds_new("age");
    config_register_rule(c, age, &age_rule);

    sds_t like = sds_new("like");
    config_register_rule(c, like, &like_rule);

    assert(dict_size(c->rules) == 3);

    sds_t value = sds_new("latte");
    assert(config_set_sds(c, "name", value) == 1);
    sds_t r = config_get_sds(c, "name");
    assert(r == value);

    assert(config_get_int64(c, "age") == DEFAULT_AGE);
    assert(config_set_int64(c, "age", 100) == 1);
    assert(config_get_int64(c, "age") == 100);
    assert(config_get_int64(c, "age") == 100);


    config_manager_delete(c);
    return 1;
}

int mockConfigFile(char* file_name, char* file_context) {
    FILE *fp = fopen(file_name,"w");
    fprintf(fp,"%s", file_context);
    fflush(fp);
    fclose(fp);
    return 1;
}

config_manager_t* test_config_manager_new() {
    config_manager_t* c = config_manager_new();
    //sds
    sds_t name = sds_new_len("name", 4);
    config_register_rule(c, name, &name_rule);
    //long long
    sds_t age = sds_new_len("age", 3);
    config_register_rule(c, age, &age_rule);

    sds_t like = sds_new("like");
    config_register_rule(c, like, &like_rule);
    return c;
}


int test_loadConfigFromString() {
    config_manager_t* c = test_config_manager_new();
    
    
    // sds_t bind = sds_new_len("bind", 4);
    // config_register_rule(c, bind, &array_sds_rule);

    char* configstr = "name a\r\nage 101\r\nlike 3 4";
    assert(load_config_from_string(c, configstr, strlen(configstr)) == 1);
    assert(strcmp(config_get_sds(c, "name"), "a") == 0);
    assert(config_get_int64(c, "age") == 101);

    char* argv[7] = {"--name", "b", "--age", "111","--like", "1", "2"};
    assert(load_config_from_argv(c, argv, 7) == 1);
    assert(strcmp(config_get_sds(c, "name"), "b") == 0);
    assert(config_get_int64(c, "age") == 111);
    struct vector_t* array = config_get_array(c, "like");
    assert(vector_size(array) == 2);
    assert(strcmp(value_get_sds(vector_get(array, 0)), "1") == 0);
    assert(strcmp(value_get_sds(vector_get(array,1)), "2") == 0);
    

    assert(mockConfigFile("name_test.config","name c\r\nage 123") == 1);
    assert(load_config_from_file(c, "name_test.config") == 1);
    assert(strcmp(config_get_sds(c, "name"), "c") == 0);

    assert(config_get_int64(c, "age") == 123);

    sds result = write_config_to_sds(c);
    
    config_manager_t* c1 = test_config_manager_new();
    load_config_from_string(c1, result, sds_len(result));
    assert(dict_size(c1->rules) == dict_size(c->rules));
    assert(sds_cmp(config_get_sds(c, "name"), config_get_sds(c1, "name")) == 0);
    
    config_manager_delete(c);
    return 1;
}



int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("config function", 
            test_configCreate() == 1);
        test_cond("load_config function",
            test_loadConfigFromString() == 1);
        
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}