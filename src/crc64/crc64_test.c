#include "test/testhelp.h"
#include "test/testassert.h"

#include <stdio.h>
#include "crc64.h"
#include "crcspeed.h"

// crc64.c
uint64_t _crc64(uint_fast64_t crc, const void *in_data, const uint64_t len);

int test_crc_64() {
    crc64_init();
    char buf[100];
    int len = sprintf(buf, "%016" PRIx64 "", (uint64_t)_crc64(0, "123456789", 9));
    assert(strncmp("e9c6d914c4b8d9ca", buf, len) == 0);
    len = sprintf(buf, "%016" PRIx64 "", (uint64_t)crc64(0, "123456789", 9));
    assert(strncmp("e9c6d914c4b8d9ca", buf, len) == 0);
    char li[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed "
                "do eiusmod tempor incididunt ut labore et dolore magna "
                "aliqua. Ut enim ad minim veniam, quis nostrud exercitation "
                "ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis "
                "aute irure dolor in reprehenderit in voluptate velit esse "
                "cillum dolore eu fugiat nulla pariatur. Excepteur sint "
                "occaecat cupidatat non proident, sunt in culpa qui officia "
                "deserunt mollit anim id est laborum.";
    len = sprintf(buf, "%016" PRIx64 "", (uint64_t)_crc64(0, li, sizeof(li)));
    assert(strncmp("c7794709e69683b3", buf, len) == 0);
    len = sprintf(buf, "%016" PRIx64 "", (uint64_t)crc64(0, li, sizeof(li)));
    assert(strncmp("c7794709e69683b3", buf, len) == 0);
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("crc64 function", 
            test_crc_64() == 1);
        
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}