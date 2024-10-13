#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <unistd.h>
#include "value.h"
int test_value() {
    value_t* v = value_new();
    value_set_int64(v, 1000L);
    assert(1000L == value_get_int64(v));
    value_set_uint64(v, 30000ULL);
    assert(30000ULL == value_get_uint64(v));
    value_set_bool(v, true);
    assert(true == value_get_bool(v));
    value_set_longdouble(v, 1.11);
    assert(1.11 == value_get_longdouble(v));
   
    value_set_sds(v, sds_new("hello"));
    assert(strcmp("hello", value_get_sds(v)) == 0);
    dict_func_t t = {
        
    };
    dict_t* d = dict_new(&t);
    value_set_map(v, d);
    assert(d == value_get_map(v));

    vector_t* ve = vector_new();
    value_set_array(v, ve);
    assert(ve == value_get_array(v));
    value_delete(v);
    return 1;
}

int test_type() {
    int64_t add = 1L;
    int64_t v0 = 9223372036854775807 + add;
    long v1 = 9223372036854775807L + add;
    long long v2 = 9223372036854775807LL + add;
    printf("sizeof int64_t :%ld %ld \n", sizeof(int64_t), v0);
    printf("sizeof long :%ld %ld\n", sizeof(long), v1);
    printf("sizeof long long :%ld %lld\n", sizeof(long long), v2);
    
    return 1;
}

int test_binary() {
    value_t* v = value_new();
    value_set_uint64(v, ULLONG_MAX);
    sds_t result = value_get_binary(v);
    
    value_t* v1 = value_new();
    value_set_binary(v1, VALUE_UINT, result, sds_len(result));
    assert(value_is_uint64(v1));
    assert(value_get_uint64(v1) == value_get_uint64(v));
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("about value function", 
            test_value() == 1);
        test_cond("about type function",
            test_type() == 1);
        test_cond("about binary function",
            test_binary() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}