/* odb_test.c
 * 
 * Latte C 库组件实现
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

/**
 * odb 单测：oio 创建，字符串/二进制/数字 写入与读取。
 */
#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <stdio.h>
#include <string.h>
#include "zmalloc/zmalloc.h"
#include "odb/odb.h"

static int test_oio_create_buffer(void) {
    oio *o = odb_oio_create_buffer();
    assert(o != NULL);
    odb_oio_free(o);
    return 1;
}

static int test_write_read_string(void) {
    oio *o = odb_oio_create_buffer();
    assert(o != NULL);
    const char *s = "hello";
    assert(odb_write_string(o, s, strlen(s)) == 1);
    odb_oio_buffer_rewind(o);
    sds got = odb_read_string(o);
    assert(got != NULL);
    assert(sds_len(got) == 5);
    assert(memcmp(got, s, 5) == 0);
    sds_delete(got);
    odb_oio_free(o);
    return 1;
}

static int test_write_read_binary(void) {
    oio *o = odb_oio_create_buffer();
    assert(o != NULL);
    const char buf[] = { 1, 2, 3, 4, 5 };
    assert(odb_write_binary(o, buf, sizeof(buf)) == 1);
    odb_oio_buffer_rewind(o);
    void *out = NULL;
    size_t len = 0;
    assert(odb_read_binary(o, &out, &len) == 0);
    assert(len == 5);
    assert(memcmp(out, buf, 5) == 0);
    zfree(out);
    odb_oio_free(o);
    return 1;
}

static int test_write_read_numbers(void) {
    oio *o = odb_oio_create_buffer();
    assert(o != NULL);
    assert(odb_write_u8(o, 0xAB) == 1);
    assert(odb_write_u16(o, 0x1234) == 1);
    assert(odb_write_u32(o, 0x12345678) == 1);
    assert(odb_write_u64(o, UINT64_C(0x123456789ABCDEF0)) == 1);
    odb_oio_buffer_rewind(o);
    uint8_t u8 = 0;
    uint16_t u16 = 0;
    uint32_t u32 = 0;
    uint64_t u64 = 0;
    assert(odb_read_u8(o, &u8) == 1 && u8 == 0xAB);
    assert(odb_read_u16(o, &u16) == 1 && u16 == 0x1234);
    assert(odb_read_u32(o, &u32) == 1 && u32 == 0x12345678);
    assert(odb_read_u64(o, &u64) == 1 && u64 == UINT64_C(0x123456789ABCDEF0));
    odb_oio_free(o);
    return 1;
}

static int test_roundtrip_mixed(void) {
    oio *o = odb_oio_create_buffer();
    assert(o != NULL);
    assert(odb_write_string(o, "world", 5) == 1);
    assert(odb_write_u32(o, 42) == 1);
    assert(odb_write_binary(o, "xyz", 3) == 1);
    assert(odb_write_u8(o, 255) == 1);
    odb_oio_buffer_rewind(o);
    sds s = odb_read_string(o);
    assert(s != NULL && sds_len(s) == 5 && memcmp(s, "world", 5) == 0);
    sds_delete(s);
    uint32_t n = 0;
    assert(odb_read_u32(o, &n) == 1 && n == 42);
    void *bin = NULL;
    size_t len = 0;
    assert(odb_read_binary(o, &bin, &len) == 0 && len == 3 && memcmp(bin, "xyz", 3) == 0);
    zfree(bin);
    uint8_t b = 0;
    assert(odb_read_u8(o, &b) == 1 && b == 255);
    odb_oio_free(o);
    return 1;
}

int main(void) {
    test_cond("oio create buffer", test_oio_create_buffer() == 1);
    test_cond("write read string", test_write_read_string() == 1);
    test_cond("write read binary", test_write_read_binary() == 1);
    test_cond("write read numbers", test_write_read_numbers() == 1);
    test_cond("roundtrip mixed", test_roundtrip_mixed() == 1);
    test_report();
    return 0;
}
