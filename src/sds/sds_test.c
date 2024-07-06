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

int test_resize() {
    sds x ;
    /* Test sdsresize - extend */
    x = sdsnew("1234567890123456789012345678901234567890");
    x = sdsResize(x, 200, 1);
    test_cond("sdsrezie() expand len", sdslen(x) == 40);
    test_cond("sdsrezie() expand strlen", strlen(x) == 40);
    test_cond("sdsrezie() expand alloc", sdsalloc(x) == 200);
    /* Test sdsresize - trim free space */
    x = sdsResize(x, 80, 1);
    test_cond("sdsrezie() shrink len", sdslen(x) == 40);
    test_cond("sdsrezie() shrink strlen", strlen(x) == 40);
    test_cond("sdsrezie() shrink alloc", sdsalloc(x) == 80);
    /* Test sdsresize - crop used space */
    x = sdsResize(x, 30, 1);
    test_cond("sdsrezie() crop len", sdslen(x) == 30);
    test_cond("sdsrezie() crop strlen", strlen(x) == 30);
    test_cond("sdsrezie() crop alloc", sdsalloc(x) == 30);
    /* Test sdsresize - extend to different class */
    x = sdsResize(x, 400, 1);
    test_cond("sdsrezie() expand len", sdslen(x) == 30);
    test_cond("sdsrezie() expand strlen", strlen(x) == 30);
    test_cond("sdsrezie() expand alloc", sdsalloc(x) == 400);
    /* Test sdsresize - shrink to different class */
    x = sdsResize(x, 4, 1);
    test_cond("sdsrezie() crop len", sdslen(x) == 4);
    test_cond("sdsrezie() crop strlen", strlen(x) == 4);
    test_cond("sdsrezie() crop alloc", sdsalloc(x) == 4);
    sdsfree(x);
    return 1;
}
int test_findlastof() {
    sds haystack = sdsnew("Hello world");
    int index = sdsFindLastOf(haystack, "Hello");
    test_cond("sds findlastof",index == 0);
    sdsfree(haystack);

    haystack = sdsnew("This is a test string with test inside.");
    index = sdsFindLastOf(haystack, "test");
    assert(index != C_NPOS);
    assert(index > 0);
    sdsfree(haystack);

    haystack = sdsnew("abcd/edf");
    index = sdsFindLastOf(haystack, "xyz");
    assert(index == C_NPOS);
    sdsfree(haystack);
    return 1;
}

int test_startsWith() {
    sds haystack = sdsnew("Hello, world!");
    assert(sdsStartsWith(haystack, "Hello"));
    sdsfree(haystack);
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
        test_cond("sdsResize function",
            test_resize() == 1);
        test_cond("sds find last of function",
            test_findlastof() == 1);
        test_cond("sds starts with function",
            test_startsWith() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}