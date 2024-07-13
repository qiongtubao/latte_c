#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <stdio.h>
#include "sds.h"
#include "sds_plugins.h"

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
    /* Test sdsresize - trim free D1 */
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

    haystack = sdsnew("/a/b/c/edf");
    index = sdsFindLastOf(haystack, "/");
    assert(index == 6);
    sdsfree(haystack);
    return 1;
}

int test_startsWith() {
    sds haystack = sdsnew("Hello, world!");
    assert(sdsStartsWith(haystack, "Hello"));
    sdsfree(haystack);
    return 1;
}

int test_fixed32() {

    char dst1[5] = {0};
    const uint32_t value = 0x12345678;
    encodeFixed32(dst1, value);
    assert(0x78 == dst1[0]);
    assert(0x56 == dst1[1]);
    assert(0x34 == dst1[2]);
    assert(0x12 == dst1[3]);
    uint32_t value1 = decodeFixed32(dst1);
    assert(0x12345678 == value1);

    sds s = sdsempty();
    s = sdsAppendFixed32(s, 0x12345678);
    assert(0x78 == s[0]);
    assert(0x56 == s[1]);
    assert(0x34 == s[2]);
    assert(0x12 == s[3]);
    value1 = decodeFixed32(s);
    assert(0x12345678 == value1);
    sdsfree(s);
    return 1;
}

int test_fixed64() {

    char dst1[8] = {0};
    const uint64_t value = 0x123456789ABCDEF0ull;
    encodeFixed64(dst1, value);
    assert(0xFFFFFFF0 == dst1[0]);
    assert(0xFFFFFFDE == dst1[1]);
    assert(0xFFFFFFBC == dst1[2]);
    assert(0xFFFFFF9A == dst1[3]);
    assert(0x78 == dst1[4]);
    assert(0x56 == dst1[5]);
    assert(0x34 == dst1[6]);
    assert(0x12 == dst1[7]);
    uint64_t value1 = decodeFixed64(dst1);
    assert(0x123456789ABCDEF0ull == value1);

    sds s = sdsempty(); 
    s = sdsAppendFixed64(s, 0x123456789ABCDEF0ull);
    value1 = decodeFixed64(s);
    assert(0xFFFFFFF0 == s[0]);
    assert(0xFFFFFFDE == s[1]);
    assert(0xFFFFFFBC == s[2]);
    assert(0xFFFFFF9A == s[3]);
    assert(0x78 == s[4]);
    assert(0x56 == s[5]);
    assert(0x34 == s[6]);
    assert(0x12 == s[7]);
    assert(0x123456789ABCDEF0ull == value1);
    sdsfree(s);
    return 1;
}

int test_varint32() {
    char buf[10];
    int len = encodeVarint32(buf, 0x12345678);
    assert(0xFFFFFFF8 == buf[0]);
    assert(0xFFFFFFAC == buf[1]);
    assert(0xFFFFFFD1 == buf[2]);
    assert(0xFFFFFF91 == buf[3]);
    assert(0x00000001 == buf[4]);
    assert(len == 5);
    len = encodeVarint32(buf + len, 0x87654321);
    assert(len == 5);
    
    assert(0xFFFFFFA1 == buf[5]);
    assert(0xFFFFFF86 == buf[6]);
    assert(0xFFFFFF95 == buf[7]);
    assert(0xFFFFFFBB == buf[8]);
    assert(0x00000008 == buf[9]);
    Slice slice;
    slice.p = buf;
    slice.len = 10;
    uint32_t value;
    assert(getVarint32(&slice, &value));
    assert(value == 0x12345678);
    assert(slice.p == buf + 5);
    assert(slice.len == 5);
    assert(getVarint32(&slice, &value));
    assert(value == 0x87654321);
    assert(slice.p == buf + 10);
    assert(slice.len == 0);
    return 1;
}

int test_varint64() {
    char buf[20];
    int len = encodeVarint64(buf, 0x123456789ABCDEF0ull);
    
    assert(len == 9);
    assert(0xFFFFFFF0 == buf[0]);
    assert(0xFFFFFFBD == buf[1]);
    assert(0xFFFFFFF3 == buf[2]);
    assert(0xFFFFFFD5 == buf[3]);
    assert(0xFFFFFF89 == buf[4]);
    assert(0xFFFFFFCF == buf[5]);
    assert(0xFFFFFF95 == buf[6]);
    assert(0xFFFFFF9A == buf[7]);
    assert(0x00000012 == buf[8]);

    len = encodeVarint64(buf + len, 0x0FEDCBA987654321ull);
    assert(len == 9);
    
    assert(0xFFFFFFA1 == buf[9]);
    assert(0xFFFFFF86 == buf[10]);
    assert(0xFFFFFF95 == buf[11]);
    assert(0xFFFFFFBB == buf[12]);
    assert(0xFFFFFF98 == buf[13]);
    assert(0xFFFFFFF5 == buf[14]);
    assert(0xFFFFFFF2 == buf[15]);
    assert(0xFFFFFFF6 == buf[16]);
    assert(0x0000000f == buf[17]);
    // len = encodeVarint32(buf + len, 0x87654321);
    Slice slice;
    slice.p = buf;
    slice.len = 20;
    uint64_t value;
    assert(getVarint64(&slice, &value));
    assert(value == 0x123456789ABCDEF0ull);
    assert(slice.p == buf + 9);
    assert(slice.len == (11));
    assert(getVarint64(&slice, &value));
    assert(value == 0x0FEDCBA987654321ull);
    assert(slice.p == buf + 18);
    assert(slice.len == 2);
    return 1;
}
int test_appendLengthprefixedSlice() {
    Slice slice;
    slice.p = "hello";
    slice.len = 5;
    sds result = sdsAppendLengthPrefixedSlice(sdsempty(), &slice);
    assert(6 == sdslen(result));
    assert(0x00000005 == result[0]);
    assert('h' == result[1]);
    return 1;
}

int test_slice() {
    Slice a = {
        .p = "hello",
        .len = 5
    };
    assert(!SliceIsEmpty(&a));
    assert(SliceData(&a) == a.p);
    assert(SliceSize(&a) == a.len);
    sds r = SliceToSds(&a);
    assert(sdslen(r) == 5);
    assert(strncmp(r, a.p, a.len) == 0);
    sdsfree(r);
    SliceRemovePrefix(&a, 1);
    Slice b = {
        .p = "ello",
        .len = 4
    };
    assert(SliceCompare(&a, &b) == 0);
    Slice c = {
        .p = "hel",
        .len = 3
    };
    assert(SliceStartsWith(&a, &c) == 0);
    SliceClear(&c);
    assert(SliceIsEmpty(&c));
    return 1;
}

int test_sdscatfmt() {
    sds d = sdsnew("test");
    sds result = sdscatfmt(sdsempty(), "%S-%i-%I-%u-%U-%%", d, 1, 110LL, 11u, 11uLL);
    assert(sdslen(result) == 18);
    assert(strncmp("test-1-110-11-11-%",result,18) == 0);

    sds o = sdsReset(d, "a", 1);
    assert(sdslen(o) == 1);
    assert(strncmp(o, "a", 1) == 0);
    // assert(d == o);
    sdsfree(o);
    sdsfree(result);
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
        test_cond("fixed32 function",
            test_fixed32() == 1);
        test_cond("fixed64 function",
            test_fixed64() == 1);
        test_cond("varint32 function",
            test_varint32() == 1);
        test_cond("varint64 function",
            test_varint64() == 1);
        test_cond("put length prefixed slice", 
            test_appendLengthprefixedSlice() == 1);
        test_cond("sdscatfmt function",
            test_sdscatfmt());
        test_cond("slice test",
            test_slice() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}