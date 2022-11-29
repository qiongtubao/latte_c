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

int test_configCreate() {
    config* c = createConfig();
    sds key = sdsnewlen("name", 4);
    registerConfig(c, key, &name_rule);
    sds value = sdsnew("latte");
    assert(configSetSds(c, "name", value) == 1);
    sds r = configGetSds(c, "name");
    assert(r == value);
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
    sds key = sdsnewlen("name", 4);
    registerConfig(c, key, &name_rule);
    char* configstr = "name a";
    assert(loadConfigFromString(c, configstr, strlen(configstr)) == 1);
    assert(strcmp(configGetSds(c, "name"), "a") == 0);
    char* argv[2] = {"--name", "b"};
    assert(loadConfigFromArgv(c, argv, 2) == 1);
    assert(strcmp(configGetSds(c, "name"), "b") == 0);

    assert(mockConfigFile("name_test.config","name c") == 1);
    assert(loadConfigFromFile(c, "name_test.config") == 1);
    assert(strcmp(configGetSds(c, "name"), "c") == 0);
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