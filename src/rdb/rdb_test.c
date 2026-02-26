#include <string.h>
#include "rdb.h"
#include <assert.h>
#include "object/object.h"
int memcmp(const void *s1, const void *s2, size_t n);

void test_primitives_buffer() {
    RedisModuleIO io;
    rdb_init_io_buffer(&io, NULL);

    assert(rdb_save_type(&io, 123) == 1);
    assert(rdb_save_len(&io, 100) == 2); /* 14 bit encoding (100 > 63) */
    assert(rdb_save_len(&io, 0x3FFF) == 2); /* 14 bit encoding */
    assert(rdb_save_len(&io, 0x10000) == 5); /* 32 bit encoding */
    assert(rdb_save_string(&io, "hello", 5) == 6); /* 1 byte len + 5 bytes data */
    
    printf("Primitives Buffer Encode Size: %llu\n", (unsigned long long)io.bytes_processed);

    /* Verify content by reading back */
    io.io.buffer.pos = 0;
    io.bytes_processed = 0;

    assert(rdb_load_type(&io) == 123);
    assert(rdb_load_len(&io, NULL) == 100);
    assert(rdb_load_len(&io, NULL) == 0x3FFF);
    assert(rdb_load_len(&io, NULL) == 0x10000);
    
    sds s = rdb_load_string(&io);
    assert(memcmp(s, "hello", 5) == 0);
    sds_delete(s);
    
    sds_delete(io.io.buffer.ptr);
    printf("Primitives Buffer Test Passed\n");
}

void test_object_string_buffer() {
    RedisModuleIO io;
    rdb_init_io_buffer(&io, NULL);
    
    sds s = sds_new("test_string");
    latte_object_t *o = latte_object_new(OBJ_STRING, s);
    
    assert(rdb_save_object(&io, o) > 0);
    
    io.io.buffer.pos = 0;
    latte_object_t *loaded = rdb_load_object(&io, OBJ_STRING);
    
    assert(loaded != NULL);
    assert(loaded->type == OBJ_STRING);
    assert(memcmp(loaded->ptr, "test_string", 11) == 0);
    
    latte_object_decr_ref_count(o);
    latte_object_decr_ref_count(loaded);
    sds_delete(io.io.buffer.ptr);
    printf("String Object Buffer Test Passed\n");
}

void test_file_io() {
    FILE *fp = fopen("test.rdb", "wb");
    assert(fp != NULL);
    
    RedisModuleIO io;
    rdb_init_io_file(&io, fp);
    
    assert(rdb_save_type(&io, 99) == 1);
    assert(rdb_save_string(&io, "file_test", 9) == 10);
    
    fclose(fp);
    
    fp = fopen("test.rdb", "rb");
    assert(fp != NULL);
    rdb_init_io_file(&io, fp);
    
    assert(rdb_load_type(&io) == 99);
    sds s = rdb_load_string(&io);
    assert(memcmp(s, "file_test", 9) == 0);
    sds_delete(s);
    
    fclose(fp);
    remove("test.rdb");
    printf("File IO Test Passed\n");
}

int main() {
    printf("Running RDB Tests...\n");
    test_primitives_buffer();
    test_object_string_buffer();
    test_file_io();
    printf("All RDB tests passed!\n");
    return 0;
}
