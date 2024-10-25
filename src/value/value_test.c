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
    value_set_long_double(v, 1.11);
    assert(1.11 == value_get_long_double(v));
   
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
    printf("sizeof int64_t :%ld %lld \n", sizeof(int64_t), v0);
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

vector_t* test_vector_new(long* data, int len) {
    vector_t* v = vector_new();
    for(int i = 0; i < len; i++) {
        value_t* va = value_new();
        value_set_int64(va, data[i]);
        vector_push(v, va);
    }
    return v;
}

uint64_t testSdsHashCallback(const void *key) {
    uint64_t result = dict_gen_hash_function(key, sds_len((sds_t)key));
    return result;
}

int testSdsCompareCallback(dict_t*privdata, const void *key1, const void *key2) {
    sds_t s1, s2;
    DICT_NOTUSED(privdata);
    s1 = (sds_t)key1;
    s2 = (sds_t)key2;
    return sds_cmp(s1, s2) == 0;
}

dict_func_t dict_ksds_vlong_func = {
    testSdsHashCallback,
    NULL,
    NULL,
    testSdsCompareCallback,
    NULL,
    NULL,
    NULL
};

dict_t* test_dict_new(char** keys, long* values, int len) {
    dict_t* d = dict_new(&dict_ksds_vlong_func);
    for(int i = 0; i< len; i++) {
        value_t* v = value_new();
        value_set_int64(v, values[i]);
        assert(dict_add(d, (void*)(sds_new(keys[i])), (void*)v) == DICT_OK);
    }
    return d;
}

int test_value_cmp() {
    value_t* a = value_new();
    value_t* b = value_new();

//constant_char
    value_set_constant_char(a, "abc");
    value_set_constant_char(b, "abc");

    assert(value_cmp(a,b) == 0);
    value_set_constant_char(b, "abcd");
    assert(value_cmp(a,b) < 0);
    value_set_constant_char(b, "ab");
    assert(value_cmp(a,b) > 0);
//int64
    value_set_int64(a, -1);
    value_set_int64(b, -1);
    assert(value_cmp(a,b) == 0);

    value_set_int64(a, -2);
    value_set_int64(b, -1);
    assert(value_cmp(a,b) < 0);

    value_set_int64(a, -2);
    value_set_int64(b, -3);
    assert(value_cmp(a,b) > 0);
//uint64
    value_set_uint64(a, 1);
    value_set_uint64(b, 1);
    assert(value_cmp(a,b) == 0);

    value_set_uint64(a, 2);
    value_set_uint64(b, 1);
    assert(value_cmp(a,b) >0);

    value_set_uint64(a, 2);
    value_set_uint64(b, 3);
    assert(value_cmp(a,b) <0);
//sds
    value_set_sds(a, sds_new("1"));
    value_set_sds(b, sds_new("1"));
    assert(value_cmp(a,b) == 0);

    value_set_sds(a, sds_new("2"));
    value_set_sds(b, sds_new("1"));
    assert(value_cmp(a,b) > 0);

    value_set_sds(a, sds_new("2"));
    value_set_sds(b, sds_new("3"));
    assert(value_cmp(a,b) < 0);
//long double
    value_set_long_double(a, 1.1);
    value_set_long_double(b, 1.1);
    assert(value_cmp(a,b) == 0);

    value_set_long_double(a, 1.1111111);
    value_set_long_double(b, 1.1);
    assert(value_cmp(a,b) > 0);

    value_set_long_double(a, 1.1111111);
    value_set_long_double(b, 1.1111112);
    assert(value_cmp(a,b) < 0);
//bool
    value_set_bool(a, true);
    value_set_bool(b, true);
    assert(value_cmp(a,b) == 0);

    value_set_bool(a, true);
    value_set_bool(b, false);
    assert(value_cmp(a,b) > 0);

    value_set_bool(a, false);
    value_set_bool(b, true);
    assert(value_cmp(a,b) < 0);
//array
    long a1[3] = {1,2,3};
    vector_t* v1 = test_vector_new(a1 , 3);
    vector_t* v2 = test_vector_new(a1, 3);
    
    
    value_set_array(a, v1);
    value_set_array(b, v2);
    assert(value_cmp(a,b) == 0);

    
    long a2[2] = {1,2}; 
    v2 = test_vector_new(a2, 2);
    value_set_array(b, v2);
    assert(value_cmp(a,b) > 0);

    long a3[4] = {1,2,3,4}; 
    v2 = test_vector_new(a3, 4);
    value_set_array(b, v2);
    assert(value_cmp(a,b) < 0);
    
    long a4[3] = {1,2,2}; 
    v2 = test_vector_new(a4, 3);
    value_set_array(b, v2);
    assert(value_cmp(a,b) > 0);

    long a5[3] = {1,2,4}; 
    v2 = test_vector_new(a5, 3);
    value_set_array(b, v2);
    assert(value_cmp(a,b) < 0);
//map
    char* k0[3] = {"1","2","3"};
    long l0[3] = {1,2,3};
    dict_t* d1 = test_dict_new(k0, l0, 3);
    dict_t* d2 = test_dict_new(k0, l0, 3);
    value_set_map(a, d1);
    value_set_map(b, d2);
    assert(value_cmp(a,b) == 0);

    char* k1[4] = {"1","2","3", "4"};
    long l1[4] = {1,2,3,4};
    d2 = test_dict_new(k1, l1, 4);
    value_set_map(b, d2);
    assert(value_cmp(a,b) < 0);

    char* k2[2] = {"1","2"};
    long l2[2] = {1,2};
    d2 = test_dict_new(k2, l2, 2);
    value_set_map(b, d2);
    assert(value_cmp(a,b) > 0);

    char* k3[3] = {"1","2","3"};
    long l3[3] = {1,2,4};
    d2 = test_dict_new(k3, l3, 3);
    value_set_map(b, d2);
    assert(value_cmp(a,b) < 0);

    char* k4[3] = {"1","2","3"};
    long l4[3] = {1,2,2};
    d2 = test_dict_new(k4, l4, 3);
    value_set_map(b, d2);
    assert(value_cmp(a,b) > 0);

    char* k5[3] = {"1","2","4"};
    long l5[3] = {1,2,3};
    d2 = test_dict_new(k5, l5, 3);
    value_set_map(b, d2);
    assert(value_cmp(a,b) < 0);

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
        test_cond("about value cmp function",
            test_value_cmp() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}