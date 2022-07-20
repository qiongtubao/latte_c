#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <stdio.h>
#include "crc64.h"
#include "crc16.h"
#include <inttypes.h>
int test_crc64(void) {
    crc64_init();
    char buf[17];
    sprintf(buf, "%016" PRIx64 ,
           (uint64_t)crc64(0, (unsigned char*)"123456789", 9));
    assert(strncmp(buf, "e9c6d914c4b8d9ca", 16) == 0);


    char li[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed "
                "do eiusmod tempor incididunt ut labore et dolore magna "
                "aliqua. Ut enim ad minim veniam, quis nostrud exercitation "
                "ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis "
                "aute irure dolor in reprehenderit in voluptate velit esse "
                "cillum dolore eu fugiat nulla pariatur. Excepteur sint "
                "occaecat cupidatat non proident, sunt in culpa qui officia "
                "deserunt mollit anim id est laborum.";
    sprintf(buf, "%016" PRIx64 ,
           (uint64_t)crc64(0, li, sizeof(li)));
    assert(strncmp(buf, "c7794709e69683b3", 16) == 0);
    return 1;
}

int test_crc16() {
    uint64_t i = crc16("123456789",9);
    char x16[5];
    sprintf(x16, "%0X", i);
    assert(strncmp(x16, "31C3", 4) == 0);
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("crc16 function", 
            test_crc16() == 1);
        test_cond("crc64 function",
            test_crc64() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}