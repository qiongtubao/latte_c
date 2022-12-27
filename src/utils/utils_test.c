#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <stdio.h>
#include "utils.h"

int test_ll2string(void) {
    char buf[100];
    assert(ll2string(buf, 100, 1234) == 4);
    return 1;
}

int test_string2ll(void) {
    char* src = "1234";
    long long ll = 0;
    assert(string2ll(src, 4, &ll) == 1);
    assert(ll == 1234);
    return 1;
}



int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("ll2string function", 
            test_ll2string() == 1);
        test_cond("string2ll function",
            test_string2ll() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}