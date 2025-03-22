#include "test/testhelp.h"
#include "test/testassert.h"
#include "cmp.h"

int test_str_cmp() {
    char* a = "123";
    char* b = "1234";
    char* c = "12";
    char* d = "123\0 4";
    assert(str_cmp((void*)a, (void*)"123") == 0);
    assert(str_cmp((void*)a, (void*)b) < 0);
    assert(str_cmp((void*)a, (void*)c) > 0);
    assert(str_cmp((void*)a, (void*)d) == 0);
    return 1;
}
int test_int64_cmp() {
    int64_t a = -3;
    int64_t b = -2;
    int64_t c = 1;
    int64_t d = -3;
    // assert(private_int64_cmp(a, d) == 0);
    // assert(private_int64_cmp(a, b) < 0);
    // assert(private_int64_cmp(c, b) > 0);

    assert(int64_cmp((void*)a, (void*)d) == 0);
    assert(int64_cmp((void*)a, (void*)b) < 0);
    assert(int64_cmp((void*)c, (void*)b) > 0);
    return 1;
}

int test_uint64_cmp() {
    uint64_t a = 1;
    uint64_t b = 2;
    uint64_t c = 3;
    uint64_t d = 1;
    // assert(private_int64_cmp(a, d) == 0);
    // assert(private_int64_cmp(a, b) < 0);
    // assert(private_int64_cmp(c, b) > 0);

    assert(uint64_cmp((void*)a, (void*)d) == 0);
    assert(uint64_cmp((void*)a, (void*)b) < 0);
    assert(uint64_cmp((void*)c, (void*)b) > 0);
    return 1;
}

int test_long_double_cmp() {
    long double a = 1;
    long double b = 1.1000;
    long double c = 1.1001;
    long double d = 1.0;
    assert(long_double_cmp((void*)&a, (void*)&d) == 0);
    assert(long_double_cmp((void*)&a, (void*)&b) < 0);
    assert(long_double_cmp((void*)&c, (void*)&b) > 0);
    return 1;
}


int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("str cmp function", 
            test_str_cmp() == 1);
        test_cond("int64 cmp function", 
            test_int64_cmp() == 1);
        test_cond("uint64 cmp function", 
            test_uint64_cmp() == 1);
        test_cond("long double cmp function", 
            test_long_double_cmp() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}