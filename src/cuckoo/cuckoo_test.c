
#include "../test/testhelp.h"
#include "../test/testassert.h"
#include "cuckoo.h"


int test_cuckoo() {
    assert(is_pow_of_2(1) == 1);
    assert(is_pow_of_2(2) == 1);
    assert(is_pow_of_2(3) == 0);
    assert(is_pow_of_2(4) == 1);
    assert(is_pow_of_2(5) == 0);
    assert(is_pow_of_2(255) == 0);
    assert(is_pow_of_2(256) == 1);

    assert(upper_pow_of_2(1) == 1);
    assert(upper_pow_of_2(2) == 2);
    assert(upper_pow_of_2(3) == 4);
    assert(upper_pow_of_2(4) == 4);
    assert(upper_pow_of_2(5) == 8);
    assert(upper_pow_of_2(6) == 8);
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("cuckoo function", 
            test_cuckoo() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}