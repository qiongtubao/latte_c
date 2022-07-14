


#include "test/testhelp.h"
#include "test/testassert.h"

#include <stdio.h>
#include "coding.h"

int test_Fixed32() {
    uint32_t count = 100000;
    sds s = sdsempty();
    assert(s != NULL);
    for (uint32_t v = 0; v < count; v++) {
        s = putSdsFixed32(s, v);
    }
    void* p = s;
    for (uint32_t v = 0; v < count; v++) {
        uint32_t actual = decodeFixed32(p);
        assert(v == actual);
        p += sizeof(uint32_t);
    }
    return 1;
}


int test_Fixed64() {
    sds s = sdsempty();
    for (int power = 0; power <= 63; power++) {
        uint64_t v = ((uint64_t)1) << power;
        s = putSdsFixed64(s, v - 1);
        s = putSdsFixed64(s, v + 0);
        s = putSdsFixed64(s, v + 1);
    }

    const char* p = s;
    for (int power = 0; power <= 63; power++) {
        uint64_t v = ((uint64_t)1) << power;
        uint64_t actual;
        actual = decodeFixed64(p);
        assert(v - 1 == actual);
        p += sizeof(uint64_t);

        actual = decodeFixed64(p);
        assert(v + 0 == actual);
        p += sizeof(uint64_t);

        actual = decodeFixed64(p);
        assert(v + 1 == actual);
        p += sizeof(uint64_t);
    }
    return 1;
}

int test_encodingOutput() {
    sds dst = sdsempty();
    dst = putSdsFixed32(dst, 0x04030201);
    assert(sdslen(dst) == 4);
    assert(0x01 == (int)(dst[0]));
    assert(0x02 == (int)(dst[1]));
    assert(0x03 == (int)(dst[2]));
    assert(0x04 == (int)(dst[3]));

    sdsclear(dst);
    dst = putSdsFixed64(dst, 0x0807060504030201ull);
    assert(sdslen(dst) == 8);
    assert(0x01 == (int)(dst[0]));
    assert(0x02 == (int)(dst[1]));
    assert(0x03 == (int)(dst[2]));
    assert(0x04 == (int)(dst[3]));
    assert(0x05 == (int)(dst[4]));
    assert(0x06 == (int)(dst[5]));
    assert(0x07 == (int)(dst[6]));
    assert(0x08 == (int)(dst[7]));

    return 1;
}

int test_varint32() {
    sds s = sdsempty();
    for (uint32_t i = 0; i < (32 * 32); i++) {
        uint32_t v = (i / 32) << (i % 32);
        s = putVarint32(s, v);
    }
    char* p = s;
    char* limit = p + sdslen(s);
    for (uint32_t i = 0; i < (32 * 32); i++) {
        uint32_t expected = (i / 32) << (i % 32);
        uint32_t actual;
        const char* start = p;
        p = getVarint32Ptr(p, limit, &actual);
        assert(p != NULL);
        assert(expected == actual);
        assert(varintLength(actual) == (p - start));
    }
    assert(p == (s + sdslen(s)));
    return 1;
}

int test_varint64() {
    uint64_t values[64 * 3 + 4];
    int index = 0;
    values[index++] = 0;
    values[index++] = 100;
    values[index++] = (~0);
    values[index++] = (~0 - 1);
    for (uint32_t k = 0; k < 64; k++) {
        const uint64_t power = 1ull << k;
        values[index++] = power;
        values[index++] = power - 1;
        values[index++] = power + 1;
    }
    sds s = sdsempty();
    for (size_t i = 0; i < index; i++) {
        s = putVarint64(s, values[i]);
    }
    const char* p = s;
    const char* limit = p + sdslen(s);
    for (size_t i = 0; i < index; i++) {
        assert(p < limit);
        uint64_t actual;
        const char* start = p;
        p = getVarint64Ptr(p, limit, &actual);
        assert(p != NULL);
        assert(values[i] == actual);
        assert(varintLength(actual) == p - start);
    }
    assert(p == limit);
    return 1;
}

int test_varint32Overflow() {
    uint32_t result;
    sds input = sdsnew("\x81\x82\x83\x84\x85\x11");
    assert(getVarint32Ptr(input, input + sdslen(input),
                             &result) == NULL);
    return 1;
}

int test_varint32Truncation() {
    uint32_t large_value = (1u << 31) + 100;
    sds s = sdsempty();
    s = putVarint32(s, large_value);
    uint32_t result;
    for (size_t len = 0; len < sdslen(s) -1; len++) {
        assert(getVarint32Ptr(s, s + len, &result) == NULL);
    }
    assert(getVarint32Ptr(s, s + sdslen(s), &result) !=
              NULL);
    assert(large_value == result);
    return 1;
}

int test_varint64Overflow() {
    uint32_t result;
    sds input = sdsnew("\x81\x82\x83\x84\x85\x81\x82\x83\x84\x85\x11");
    assert(getVarint64Ptr(input, input + sdslen(input),
                             &result) == NULL);
    return 1;
}

int test_varint64Truncation() {
    uint64_t large_value = (1ull << 63) + 100ull;
    sds s = sdsempty();
    s = putVarint64(s, large_value);
    uint64_t result;
    for (size_t len = 0; len < sdslen(s) -1; len++) {
        assert(getVarint64Ptr(s, s + len, &result) == NULL);
    }
    assert(getVarint64Ptr(s, s + sdslen(s), &result) !=
              NULL);
    assert(large_value == result);
    return 1;
}


int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("Fixed32 function", 
            test_Fixed32() == 1);
        test_cond("Fixed64 function",
            test_Fixed64() == 1);
        test_cond("encodingOutput function",
            test_encodingOutput() == 1);
        test_cond("varint32 function",
            test_varint32() == 1);
        test_cond("varint64 function",
            test_varint64() == 1);
        test_cond("varint32 overflow",
            test_varint32Overflow() == 1);
        test_cond("varint32 truncation",
            test_varint32Truncation() == 1);
        test_cond("varint64 overflow",
            test_varint64Overflow() == 1);
        test_cond("varint64 truncation",
            test_varint64Truncation() == 1);
        
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}