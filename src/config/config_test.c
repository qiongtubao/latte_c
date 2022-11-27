#include "test/testhelp.h"
#include "test/testassert.h"

#include <stdio.h>
#include "config.h"
#include "zmalloc/zmalloc.h"
#include <sys/time.h>

int nameInit(configRule* rule, void* value) {
    rule->value = value;
    return 1;
}

int nameUpdate(configRule* rule, void* old_value, void* new_value) {
    rule->value = new_value;
    return 1;
}

sds nameSds(configRule* rule) {
    return sdsnewlen(rule->value, sdslen(rule->value));
}

void nameReleaseValue(void* value) {
    if(value) {
        sdsfree(value);
    }
}

configRule name_rule = {
    .init = nameInit,
    .update = nameUpdate,
    .toSds = nameSds,
    .releaseValue = nameReleaseValue
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

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("config function", 
            test_configCreate() == 1);
        
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}