#include "test/testhelp.h"
#include "test/testassert.h"

#include <stdio.h>
#include "config.h"
#include "zmalloc/zmalloc.h"
#include <sys/time.h>




configRule name_rule = {
    .update = sdsUpdate,
    .writeConfigSds = sdsWriteConfigSds,
    .releaseValue =sdsReleaseValue,
    .load = sdsLoadConfig
};

#define DEFAULT_AGE ((void*)10)

configRule age_rule = {
    .update = longLongUpdate,
    .writeConfigSds = longLongWriteConfigSds,
    .releaseValue = longLongReleaseValue,
    .load = longLongLoadConfig,
    .value = DEFAULT_AGE
};

configRule array_sds_rule = {
    .update = arraySdsUpdate,
    .writeConfigSds = arraySdsWriteConfigSds,
    .releaseValue = arraySdsReleaseValue,
    .load = arraySdsLoadConfig
};


int test_configCreate() {
    config* c = createConfig();
    //sds
    sds name = sdsnew("name");
    registerConfig(c, name, &name_rule);
    //long long
    sds age = sdsnew("age");
    registerConfig(c, age, &age_rule);

    assert(dictSize(c->rules) == 2);

    sds value = sdsnew("latte");
    assert(configSetSds(c, "name", value) == 1);
    sds r = configGetSds(c, "name");
    assert(r == value);

    assert(configGetLongLong(c, "age") == DEFAULT_AGE);
    assert(configSetLongLong(c, "age", 100) == 1);
    assert(configGetLongLong(c, "age") == 100);
    assert(configGetInt(c, "age") == 100);


    releaseConfig(c);
    return 1;
}

int mockConfigFile(char* file_name, char* file_context) {
    FILE *fp = fopen(file_name,"w");
    fprintf(fp,"%s", file_context);
    fflush(fp);
    fclose(fp);
    return 1;
}

int test_loadConfigFromString() {
    config* c = createConfig();
    //sds
    sds name = sdsnewlen("name", 4);
    registerConfig(c, name, &name_rule);
    //long long
    sds age = sdsnewlen("age", 3);
    registerConfig(c, age, &age_rule);

    sds bind = sdsnewlen("bind", 4);
    registerConfig(c, bind, &array_sds_rule);

    char* configstr = "name a\r\nage 101";
    assert(loadConfigFromString(c, configstr, strlen(configstr)) == 1);
    assert(strcmp(configGetSds(c, "name"), "a") == 0);
    assert(configGetLongLong(c, "age") == 101);

    char* argv[7] = {"--name", "b", "--age", "111","--bind", "1", "2"};
    assert(loadConfigFromArgv(c, argv, 7) == 1);
    assert(strcmp(configGetSds(c, "name"), "b") == 0);
    assert(configGetLongLong(c, "age") == 111);
    struct arraySds* array = configGetArray(c, "bind");
    assert(array->len == 2);
    assert(strcmp((array->value[0]), "1") == 0);
    assert(strcmp((array->value[1]), "2") == 0);
    

    assert(mockConfigFile("name_test.config","name c\r\n age 123") == 1);
    assert(loadConfigFromFile(c, "name_test.config") == 1);
    assert(strcmp(configGetSds(c, "name"), "c") == 0);

    assert(configGetLongLong(c, "age") == 123);
    releaseConfig(c);
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