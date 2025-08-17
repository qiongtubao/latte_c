

#include "test/testhelp.h"
#include "test/testassert.h"

#include <stdio.h>
#include "config.h"
#include "zmalloc/zmalloc.h"
#include <sys/time.h>
#include "log/log.h"


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
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}