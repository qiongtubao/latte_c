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

int test_crc64() {
    crc64_init();
    crc64_init();
    // printf("[64speed]: e9c6d914c4b8d9ca == %016" PRIx64 "\n",
    //        (uint64_t)crc64(0, (unsigned char*)"123456789", 9));
    assert(crc64(0, "123456789", 9) == 0xe9c6d914c4b8d9ca);
    char li[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed "
                "do eiusmod tempor incididunt ut labore et dolore magna "
                "aliqua. Ut enim ad minim veniam, quis nostrud exercitation "
                "ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis "
                "aute irure dolor in reprehenderit in voluptate velit esse "
                "cillum dolore eu fugiat nulla pariatur. Excepteur sint "
                "occaecat cupidatat non proident, sunt in culpa qui officia "
                "deserunt mollit anim id est laborum.";

    // printf("[64speed]: c7794709e69683b3 == %016" PRIx64 "\n",
    //        (uint64_t)crc64(0, (unsigned char*)li, sizeof(li)));
    assert((uint64_t)crc64(0, (unsigned char*)li, sizeof(li)) == 0xc7794709e69683b3);
    return 1;
}

int test_crc32jamcrc() {
    assert(0x340bc6d9 == crc32jamcrc("123456789", 9));
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
            test_crc64() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}