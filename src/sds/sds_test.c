#include "test/testhelp.h"
#include "test/testassert.h"
#include <stdio.h>
#include "sds.h"

int test_sdsnew(void) {
    sds s = sdsnew("foo");
    assert(strcmp(s, "foo") == 0);
    sdsfree(s);
    return 1;
}

int test_sdslen(void) {
    sds x = sdsnew("foo");
    assert(sdslen(x) == 3);
    assert(memcmp(x, "foo\0", 4) == 0);
    return 1;
}

int test_sdsnewlen(void) {
    sds x = sdsnewlen("foo", 2);
    assert(sdslen(x) == 2);
    assert(memcmp(x, "fo\0", 3) == 0);
    return 1;
}

int test_sdscat(void) {
    sds x = sdsnewlen("foo", 2);
    x = sdscat(x, "bar");
    assert(sdslen(x) == 5);
    assert(memcmp(x, "fobar\0", 6) == 0);
    return 1;
}

int test_sdsempty(void) {
    sds empty = sdsempty();
    assert(sdslen(empty) == 0);
    return 1;
}

int test_sdscatfmt(void) {
    sds x = sdsempty();
    x = sdscatfmt(x, "string:%s", "hello");
    assert(memcmp(x, "string:hello\0", strlen("string:hello\0")) == 0);
    sdsfree(x);
    x = sdsempty();
    x = sdscatfmt(x, "int:%i", 100);
    assert(memcmp(x, "int:100\0", strlen("int:100\0")) == 0);
    sdsfree(x);
    x = sdsempty();
    x = sdscatfmt(x, "long:%u", 2147483648);
    assert(memcmp(x, "long:2147483648\0", strlen("long:2147483648\0")) == 0);
    return 1;
}

int test_sdssplitlen(void) {
    int len = 0;
    sds* splits = sdssplitlen("test_a_b_c", 10, "_", 1, &len);
    assert(len == 4);
    assert(memcmp(splits[0], "test\0",5) == 0);
    assert(memcmp(splits[1], "a\0",2) == 0);
    assert(memcmp(splits[2], "b\0",2) == 0);
    assert(memcmp(splits[3], "c\0",2) == 0);
    sdsfreesplitres(splits, len);
    return 1;
}

int test_sdscmp(void) {
    sds x = sdsnew("hello");
    sds y = sdsnew("hello");
    assert(sdscmp(x, y) == 0);
    return 1;
}

int test_sdsfromlonglong(void) {
    sds ll = sdsfromlonglong(100);
    assert(memcmp(ll, "100\0", 4) == 0);
    return 1;
}

int test_sdscatsds(void) {
    sds x = sdsnew("x");
    sds y = sdsnew("y");
    x = sdscatsds(x, y);
    assert(memcmp(x, "xy\0", 3) == 0);
    return 1;
}

int test_sdstrim(void) {
    sds s = sdsnew("AA...AA.a.aa.aHelloWorld     :::");
    s = sdstrim(s,"Aa. :");
    assert(memcmp(s, "HelloWorld\0", 11) == 0);
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("sdsnew function", 
            test_sdsnew() == 1);
        test_cond("sdslen function", 
            test_sdslen() == 1);
        test_cond("sdsnewlen function", 
            test_sdsnewlen() == 1);
        test_cond("sdscat function", 
            test_sdscat() == 1);
        test_cond("sdsempty function",
            test_sdsempty() == 1);
        test_cond("sdscatfmt function",
            test_sdscatfmt() == 1);
        test_cond("sdssplitlen function",
            test_sdssplitlen() == 1);
        test_cond("sdscmp function",
            test_sdscmp() == 1);
        test_cond("sdsfromlonglong function",
            test_sdsfromlonglong() == 1);
        test_cond("sdscatsds function",
            test_sdscatsds() == 1);
        test_cond("sdstrim function",
            test_sdstrim() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}