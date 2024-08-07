#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <stdio.h>
#include "sds.h"

int test_sdsnew(void) {
    //type 5
    sds s = sdsnew("foo");
    assert(strcmp(s, "foo") == 0);
    sdsfree(s);
    return 1;
}

int assert_random_sds(size_t len, int try) {
    char* buf = zmalloc(len);
    for(size_t i = 0; i < len; i++) {
        buf[i] = 'a';
    }
    sds s;
    if (try) {
        s = sdstrynewlen(buf ,len);
        assert(s != NULL);
    } else {
        s = sdsnewlen(buf, len);
    }
    
    assert(strncmp(s, buf, len) == 0);
    sdsfree(s);
    zfree(buf);
    return 1;
}

int test_sdsnewlen(void) {
    //null
    sds value = sdsnewlen(NULL, 100);
    assert(value != NULL);
    //type 5
    assert(assert_random_sds((1 << 5) - 1, 0) == 1);
    //type 8
    assert(assert_random_sds((1 << 8) - 1, 0) == 1);

    //type16
    assert(assert_random_sds((1 << 16) - 1, 0) == 1);

    //type32
    assert(assert_random_sds((1ll << 32) - 1, 0) == 1);

    //type64
    assert(assert_random_sds((1ll << 32), 0) == 1);
    
    sdsfree(value);
    return 1;
}

int test_sdstrynewlen() {
    //null
    sds value = sdstrynewlen(NULL, 100);
    assert(value != NULL);
    //type 5
    assert(assert_random_sds((1 << 5) - 1, 1) == 1);
    //type 8
    assert(assert_random_sds((1 << 8) - 1, 1) == 1);

    //type16
    assert(assert_random_sds((1 << 16) - 1, 1) == 1);

    //type32
    assert(assert_random_sds((1ll << 32) - 1, 1) == 1);

    //type64
    assert(assert_random_sds((1ll << 32), 1) == 1);

    sdsfree(value);
    return 1;
}


int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("sdsnew function", 
            test_sdsnew() == 1);
        test_cond("sdsnewlen function",
            test_sdsnewlen() == 1);
        test_cond("sdstrynewlen function",
            test_sdstrynewlen() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}