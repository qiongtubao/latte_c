#include "test/testhelp.h"
#include "test/testassert.h"
#include "cmp.h"
int test_int64_cmp() {
    int64_t a = 1;
    int64_t b = 2;
    int64_t c = 3;
    int64_t d = 1;
    assert(private_int64_cmp(a, d) == 0);
    assert(private_int64_cmp(a, b) < 0);
    assert(private_int64_cmp(c, b) > 0);

    assert(int64_cmp((void*)a, (void*)d) == 0);
    assert(int64_cmp((void*)a, (void*)b) < 0);
    assert(int64_cmp((void*)c, (void*)b) > 0);
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("cmp function", 
            test_int64_cmp() == 1);
        
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}