
#include "endianconv.h"
#include "test/testhelp.h"
#include "test/testassert.h"


int test_memrev16() {
    char buf[32];
    sprintf(buf,"ciaoroma");
    memrev16(buf);
    assert(strcmp(buf, "icaoroma") == 0);
    return 1;
}

int test_memrev32() {
    char buf[32];
    sprintf(buf,"ciaoroma");
    memrev32(buf);
    assert(strcmp(buf, "oaicroma") == 0);
    return 1;
}

int test_memrev64() {
    char buf[32];
    sprintf(buf,"ciaoroma");
    memrev64(buf);
    assert(strcmp(buf, "amoroaic") == 0);
    return 1;
}

int test_intrev16() {
    uint16_t v = 100;
    uint16_t r = intrev16(v);
    assert(r == 25600);
    return 1;
}

int test_intrev32() {
    uint32_t v = 100;
    uint32_t r = intrev32(v);
    assert(r == 1677721600);
    return 1;
}

int test_intrev64() {
    uint64_t v = 100;
    uint64_t r = intrev64(v);
    assert(r == 7205759403792793600);
    printf("%lld \n", r);

    return 1;
}
int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("memrev16 function", 
            test_memrev16() == 1);
        test_cond("memrev32 function", 
            test_memrev32() == 1);
        test_cond("memrev64 function", 
            test_memrev64() == 1);
        test_cond("intrev16 function",
            test_intrev16() == 1);
        test_cond("intrev32 function",
            test_intrev32() == 1);
        test_cond("intrev64 function",
            test_intrev64() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}