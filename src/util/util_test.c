#include "test/testhelp.h"
#include "test/testassert.h"
#include "util.h"

#ifdef LATTE_TEST
    // ..... 
#endif

int test_ll2string() {
    char buf[21];
    int len = ll2string(buf, 21, 1);
    assert(strncmp(buf, "1", len) == 0);
    return 1;
}

int test_string2ll() {
    char* value_str = "1";
    long long value = 0;
    assert(string2ll(value_str, strlen(value_str), &value) == 1);
    assert(value == 1);
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // .....
        #endif
        test_cond("ll2string function", 
            test_ll2string() == 1);
        test_cond("string2ll function", 
            test_string2ll() == 1);
    } test_report()
    return 1;
}

int main(void) {
    test_api();
    return 0;
}