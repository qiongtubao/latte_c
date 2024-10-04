#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <stdio.h>
#include "utils.h"
#include "atomic.h"

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

int test_atomic() {
    latteAtomic int a = 0;
    int r ;
    atomicGet(a, r);
    assert(r == 0);
    atomicGetIncr(a , r, 1);
    assert(r == 0);
    atomicGet(a, r);
    assert(r == 1);
    atomicIncr(a ,1);
    atomicGet(a, r);
    assert(r == 2);
    atomicDecr(a, 1);
    atomicGet(a, r);
    assert(r == 1);
    return 1;
}

int test_d2string() {
    double d = 1.21;
    sds result = d2sds(d);
    assert(strcmp(result, "1.21") == 0);

    double d1 = 0;
    assert(sds2d(result, &d1) == 1);
    assert(d == d1);
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
        test_cond("atomic function",
            test_atomic() == 1);
        test_cond("string2d + 2dstring function",
            test_d2string() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}