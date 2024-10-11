#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <stdio.h>
#include "utils.h"
#include "atomic.h"

int test_ll_string(void) {
    // ll => string
    char buf[MAX_ULL_CHARS];
    assert(ll2string(buf, MAX_ULL_CHARS, 1234) == 4);

    sds ll_str = ll2sds(1234);
    assert(ll_str != NULL);
    assert(sdslen(ll_str) == 4);
    assert(strncmp("1234", ll_str, 4) == 0);


    // string => ll
    char* src = "1234";
    long long ll = 0;
    assert(string2ll(src, 4, &ll) == 1);
    assert(ll == 1234);

    ll = 0;
    assert(sds2ll(ll_str, &ll) == 1);
    assert(ll == 1234);

    sdsfree(ll_str);
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

int test_d_string() {
    // d => string
    double d = 1.21;
    char buffer[MAX_LONG_DOUBLE_CHARS];
    int len = d2string(buffer, MAX_LONG_DOUBLE_CHARS, d);
    assert(len == 4);
    assert(strncmp(buffer, "1.21", len) == 0);
    
    sds result = d2sds(d);
    assert(strcmp(result, "1.21") == 0);


    // string => d
    double d1 = 0;
    assert(string2d(buffer, len, &d1) == 1);
    assert(d == d1);

    d1 = 0;
    assert(sds2d(result, &d1) == 1);
    assert(d == d1);
    return 1;
}

int test_ld_string() {
    // ld => string
    long double  ld = 1.234;
    sds result = ld2sds(ld, 0);
    assert(strcmp(result, "1.234") == 0);


    // string => ld
    long double ld1 = 0;
    assert(sds2ld(result, &ld1) == 1);
    assert(ld - ld1 < 0.001);
    return 1;
}



int test_ull_string() {
    //ull => string
    uint64_t u = UINT64_MAX;
    char buffer[MAX_ULL_CHARS];
    int len = ull2string(buffer, MAX_ULL_CHARS, u);
    assert(len == 20);
    assert(strncmp("18446744073709551615", buffer, 20) == 0);

    sds u_str = ull2sds(u);
    assert(sdslen(u_str) == 20);
    assert(strncmp("18446744073709551615", u_str, 20) == 0);

    // string => ull
    uint64_t v = 0; 
    assert(1 == string2ull("12345", &v));
    assert(v == 12345);

    assert(1 == string2ull(buffer, &v));
    assert(v == UINT64_MAX);

    

    return 1;
}


int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        
        test_cond("atomic function",
            test_atomic() == 1);
        test_cond("string <=> d + d2string function",
            test_d_string() == 1);
        test_cond("string <=> ld + ld2string function",
            test_ld_string() == 1);
        test_cond("string <=> ll function", 
            test_ll_string() == 1);
        test_cond("string <=> ull function",
            test_ull_string() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}