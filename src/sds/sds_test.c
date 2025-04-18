#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <stdio.h>
#include "sds.h"
#include "sds_plugins.h"

int test_sds_new(void) {
    //type 5
    sds_t s = sds_new("foo");
    assert(strcmp(s, "foo") == 0);
    sds_delete(s);
    return 1;
}

int assert_random_sds(size_t len, int try) {
    char* buf = zmalloc(len);
    for(size_t i = 0; i < len; i++) {
        buf[i] = 'a';
    }
    sds_t s;
    if (try) {
        s = sds_try_new_len(buf ,len);
        assert(s != NULL);
    } else {
        s = sds_new_len(buf, len);
    }
    
    assert(strncmp(s, buf, len) == 0);
    sds_delete(s);
    zfree(buf);
    return 1;
}

int test_sds_new_len(void) {
    //null
    sds_t value = sds_new_len(NULL, 100);
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
    
    sds_delete(value);
    return 1;
}

int test_sds_try_new_len() {
    //null
    sds_t value = sds_try_new_len(NULL, 100);
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

    sds_delete(value);
    return 1;
}

int test_resize() {
    sds_t x ;
    /* Test sdsresize - extend */
    x = sds_new("1234567890123456789012345678901234567890");
    x = sds_resize(x, 200, 1);
    test_cond("sdsrezie() expand len", sds_len(x) == 40);
    test_cond("sdsrezie() expand strlen", strlen(x) == 40);
    test_cond("sdsrezie() expand alloc", sds_alloc(x) == 200);
    /* Test sdsresize - trim free D1 */
    x = sds_resize(x, 80, 1);
    test_cond("sdsrezie() shrink len", sds_len(x) == 40);
    test_cond("sdsrezie() shrink strlen", strlen(x) == 40);
    test_cond("sdsrezie() shrink alloc", sds_alloc(x) == 80);
    /* Test sdsresize - crop used space */
    x = sds_resize(x, 30, 1);
    test_cond("sdsrezie() crop len", sds_len(x) == 30);
    test_cond("sdsrezie() crop strlen", strlen(x) == 30);
    test_cond("sdsrezie() crop alloc", sds_alloc(x) == 30);
    /* Test sdsresize - extend to different class */
    x = sds_resize(x, 400, 1);
    test_cond("sdsrezie() expand len", sds_len(x) == 30);
    test_cond("sdsrezie() expand strlen", strlen(x) == 30);
    test_cond("sdsrezie() expand alloc", sds_alloc(x) == 400);
    /* Test sdsresize - shrink to different class */
    x = sds_resize(x, 4, 1);
    test_cond("sdsrezie() crop len", sds_len(x) == 4);
    test_cond("sdsrezie() crop strlen", strlen(x) == 4);
    test_cond("sdsrezie() crop alloc", sds_alloc(x) == 4);
    sds_delete(x);
    return 1;
}
int test_findlastof() {
    sds_t haystack = sds_new("Hello world");
    size_t index = sds_find_lastof(haystack, "Hello");
    test_cond("sds_t findlastof",index == 0);
    sds_delete(haystack);

    haystack = sds_new("This is a test string with test inside.");
    index = sds_find_lastof(haystack, "test");
    assert(index != C_NPOS);
    assert(index > 0);
    sds_delete(haystack);

    haystack = sds_new("abcd/edf");
    index = sds_find_lastof(haystack, "xyz");
    assert(index == C_NPOS);
    sds_delete(haystack);

    haystack = sds_new("/a/b/c/edf");
    index = sds_find_lastof(haystack, "/");
    assert(index == 6);
    sds_delete(haystack);
    return 1;
}

int test_startsWith() {
    sds_t haystack = sds_new("Hello, world!");
    assert(sds_starts_with(haystack, "Hello"));
    sds_delete(haystack);
    return 1;
}

int test_fixed32() {

    char dst1[5] = {0};
    const uint32_t value = 0x12345678;
    encode_fixed32(dst1, value);
    assert(0x78 == (unsigned int)dst1[0]);
    assert(0x56 == (unsigned int)dst1[1]);
    assert(0x34 == (unsigned int)dst1[2]);
    assert(0x12 == (unsigned int)dst1[3]);
    uint32_t value1 = decode_fixed32(dst1);
    assert(0x12345678 == value1);

    sds_t s = sds_empty();
    s = sds_append_fixed32(s, 0x12345678);
    assert(0x78 == (unsigned int)s[0]);
    assert(0x56 == (unsigned int)s[1]);
    assert(0x34 == (unsigned int)s[2]);
    assert(0x12 == (unsigned int)s[3]);
    value1 = decode_fixed32(s);
    assert(0x12345678 == value1);
    sds_delete(s);
    return 1;
}

int test_fixed64() {

    char dst1[8] = {0};
    const uint64_t value = 0x123456789ABCDEF0ull;
    encode_fixed64(dst1, value);
    assert(0xFFFFFFF0 == (unsigned int)dst1[0]);
    assert(0xFFFFFFDE == (unsigned int)dst1[1]);
    assert(0xFFFFFFBC == (unsigned int)dst1[2]);
    assert(0xFFFFFF9A == (unsigned int)dst1[3]);
    assert(0x78 == (unsigned int)dst1[4]);
    assert(0x56 == (unsigned int)dst1[5]);
    assert(0x34 == (unsigned int)dst1[6]);
    assert(0x12 == (unsigned int)dst1[7]);
    uint64_t value1 = decode_fixed64(dst1);
    assert(0x123456789ABCDEF0ull == value1);

    sds_t s = sds_empty(); 
    s = sds_append_fixed64(s, 0x123456789ABCDEF0ull);
    value1 = decode_fixed64(s);
    assert(0xFFFFFFF0 == (unsigned int)s[0]);
    assert(0xFFFFFFDE == (unsigned int)s[1]);
    assert(0xFFFFFFBC == (unsigned int)s[2]);
    assert(0xFFFFFF9A == (unsigned int)s[3]);
    assert(0x78 == (unsigned int)s[4]);
    assert(0x56 == (unsigned int)s[5]);
    assert(0x34 == (unsigned int)s[6]);
    assert(0x12 == (unsigned int)s[7]);
    assert(0x123456789ABCDEF0ull == value1);
    sds_delete(s);
    return 1;
}

int test_varint32() {
    char buf[10];
    int len = encode_var_int32(buf, 0x12345678);
    assert(0xFFFFFFF8 == (unsigned int)buf[0]);
    assert(0xFFFFFFAC == (unsigned int)buf[1]);
    assert(0xFFFFFFD1 == (unsigned int)buf[2]);
    assert(0xFFFFFF91 == (unsigned int)buf[3]);
    assert(0x00000001 == (unsigned int)buf[4]);
    assert(len == 5);
    len = encode_var_int32(buf + len, 0x87654321);
    assert(len == 5);
    
    assert(0xFFFFFFA1 == (unsigned int)buf[5]);
    assert(0xFFFFFF86 == (unsigned int)buf[6]);
    assert(0xFFFFFF95 == (unsigned int)buf[7]);
    assert(0xFFFFFFBB == (unsigned int)buf[8]);
    assert(0x00000008 == (unsigned int)buf[9]);
    slice_t slice;
    slice.p = buf;
    slice.len = 10;
    uint32_t value;
    assert(get_var_int32(&slice, &value));
    assert(value == 0x12345678);
    assert(slice.p == buf + 5);
    assert(slice.len == 5);
    assert(get_var_int32(&slice, &value));
    assert(value == 0x87654321);
    assert(slice.p == buf + 10);
    assert(slice.len == 0);
    return 1;
}

int test_varint64() {
    char buf[20];
    int len = encode_var_int64(buf, 0x123456789ABCDEF0ull);
    
    assert(len == 9);
    assert(0xFFFFFFF0 == (unsigned int)buf[0]);
    assert(0xFFFFFFBD == (unsigned int)buf[1]);
    assert(0xFFFFFFF3 == (unsigned int)buf[2]);
    assert(0xFFFFFFD5 == (unsigned int)buf[3]);
    assert(0xFFFFFF89 == (unsigned int)buf[4]);
    assert(0xFFFFFFCF == (unsigned int)buf[5]);
    assert(0xFFFFFF95 == (unsigned int)buf[6]);
    assert(0xFFFFFF9A == (unsigned int)buf[7]);
    assert(0x00000012 == (unsigned int)buf[8]);

    len = encode_var_int64(buf + len, 0x0FEDCBA987654321ull);
    assert(len == 9);
    
    assert(0xFFFFFFA1 == (unsigned int)buf[9]);
    assert(0xFFFFFF86 == (unsigned int)buf[10]);
    assert(0xFFFFFF95 == (unsigned int)buf[11]);
    assert(0xFFFFFFBB == (unsigned int)buf[12]);
    assert(0xFFFFFF98 == (unsigned int)buf[13]);
    assert(0xFFFFFFF5 == (unsigned int)buf[14]);
    assert(0xFFFFFFF2 == (unsigned int)buf[15]);
    assert(0xFFFFFFF6 == (unsigned int)buf[16]);
    assert(0x0000000f == (unsigned int)buf[17]);
    // len = encode_var_int32(buf + len, 0x87654321);
    slice_t slice;
    slice.p = buf;
    slice.len = 20;
    uint64_t value;
    assert(get_var_int64(&slice, &value));
    assert(value == 0x123456789ABCDEF0ull);
    assert(slice.p == buf + 9);
    assert(slice.len == (11));
    assert(get_var_int64(&slice, &value));
    assert(value == 0x0FEDCBA987654321ull);
    assert(slice.p == buf + 18);
    assert(slice.len == 2);
    return 1;
}
int test_appendLengthprefixedSlice() {
    slice_t slice;
    slice.p = "hello";
    slice.len = 5;
    sds_t result = sds_append_length_prefixed_slice(sds_empty(), &slice);
    assert(6 == sds_len(result));
    assert(0x00000005 == result[0]);
    assert('h' == result[1]);
    return 1;
}

int test_slice() {
    slice_t a = {
        .p = "hello",
        .len = 5
    };
    assert(!slice_is_empty(&a));
    assert(slice_data(&a) == a.p);
    assert(slice_size(&a) == (size_t)a.len);
    sds_t r = slice_to_sds(&a);
    assert(sds_len(r) == 5);
    assert(strncmp(r, a.p, a.len) == 0);
    sds_delete(r);
    slice_remove_prefix(&a, 1);
    slice_t b = {
        .p = "ello",
        .len = 4
    };
    assert(slice_compare(&a, &b) == 0);
    slice_t c = {
        .p = "hel",
        .len = 3
    };
    assert(slice_starts_with(&a, &c) == 0);
    slice_clear(&c);
    assert(slice_is_empty(&c));

    slice_t d = {
        .p = "hello",
        .len = 5
    };
    sds_t result = sds_append_length_prefixed_slice(sds_empty(), &d);
    assert(sds_len(result) == 6);
    slice_t input = {
        .p = result,
        .len = sds_len(result),
    };
    slice_t output = {.p = NULL, .len = 0};
 
    assert(true == get_length_prefixed_slice(&input, &output));
    assert(output.len == 5);
    assert(input.len == 0);
    assert(strncmp(output.p , "hello", 5)== 0);
    return 1;
}

int test_sds_cat_fmt() {
    sds_t d = sds_new("test");
    sds_t result = sds_cat_fmt(sds_empty(), "%S-%i-%I-%u-%U-%%", d, 1, 110LL, 11u, 11uLL);
    assert(sds_len(result) == 18);
    assert(strncmp("test-1-110-11-11-%",result,18) == 0);

    sds_t o = sds_reset(d, "a", 1);
    assert(sds_len(o) == 1);
    assert(strncmp(o, "a", 1) == 0);
    // assert(d == o);
    sds_delete(o);
    sds_delete(result);
    return 1;
}

int test_memcpy() {
    sds_t result = sds_new_len("", 5);
    assert(5 == sds_len(result));
    printf("result == %s\n", result);
    memcpy(result, "hello", 5);
    assert(5 == sds_len(result));
    assert(strncmp(result, "hello", 5) == 0);
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("sds_new function", 
            test_sds_new() == 1);
        test_cond("sds_new_len function",
            test_sds_new_len() == 1);
        test_cond("sds_try_new_len function",
            test_sds_try_new_len() == 1);
        test_cond("sds_resize function",
            test_resize() == 1);
        test_cond("sds_t find last of function",
            test_findlastof() == 1);
        test_cond("sds_t starts with function",
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
        test_cond("sds_cat_fmt function",
            test_sds_cat_fmt());
        test_cond("slice test",
            test_slice() == 1);
        test_cond("reset sds",
            test_memcpy() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}