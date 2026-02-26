#include "test/testhelp.h"
#include "test/testassert.h"
#include "crc.h"
int test_crc16xmodem() {
    //CRC-16/XMODEM
    assert(0x31c3 == crc16xmodem("123456789", 9));
    return 1;
}

int test_crc32c() {
    //CRC-32c
    assert(0xe3069283 == crc32c("123456789", 9));
    assert(0xe3069283 == crc32c_extend(0, "123456789", 9)); 
    assert(0xc78ab0e5 == crc32c_mask(0xE3069283));

    assert(crc32c_unmask(0xc78ab0e5) == 0xE3069283);

    //crc32c([0..n]) = crc32extend(crc32[0], crc32c(1..n))
    uint32_t a = crc32c("1", 1);
    a = crc32c_extend(a, "23456789", 8);
    assert(a == 0xe3069283);
    return 1;
}



int test_crc32jamcrc() {
    assert(0x340bc6d9 == crc32jamcrc("123456789", 9));
    return 1;
}

int test_crc64_redis() {
    
    char *data = "123456789";
    // This value needs to be verified against the specific Jones implementation.
    // Standard Jones CRC64 initialization is often 0.
    // Common check value for CRC-64-Jones ("123456789", init=0) is 0xe9c6d914c4b8d9ca.
    // However, Redis init is 0. Let's assume standard behavior first.
    // If it fails, I will debug.
    uint64_t res = crc64_redis(0, (unsigned char*)data, 9);
    printf("CRC64: %llx\n", (unsigned long long)res);
    // e9c6d914c4b8d9ca redis crc64
    assert(0xe9c6d914c4b8d9ca == res);
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("crc16-crc16xmodem function", 
            test_crc16xmodem() == 1);
        test_cond("crc32c function", 
            test_crc32c() == 1); 
        test_cond("crc32 jamcrc function", 
            test_crc32jamcrc() == 1);
        test_cond("crc64 function", 
            test_crc64_redis() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}