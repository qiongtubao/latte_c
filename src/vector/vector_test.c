//
// Created by dong on 23-5-22.
//

#include "test/testhelp.h"
#include "test/testassert.h"

#include <stdio.h>
#include "zmalloc/zmalloc.h"
#include "vector.h"
#include <unistd.h>

int test_vector() {
    vector_t* v = vector_new();
    assert(v != NULL);
    vector_push(v, (void*)1);
    assert(vector_get(v, 0) == (void*)1);
    vector_push(v, (void*)2);
    assert(vector_get(v, 1) == (void*)2);

    int a[2] = {1,2};
    latte_iterator_t* iterator = vector_get_iterator(v);
    int index = 0;
    while(latte_iterator_has_next(iterator)) {
        void* r = latte_iterator_next(iterator);
        assert((long)r == a[index++]);
    }
    latte_iterator_delete(iterator);


    assert(vector_size(v) == 2);
    vector_set(v, 1, (void*)3);
    assert(vector_get(v, 1) == (void*)3);

    assert(vector_pop(v) == (void*)3);

    assert(vector_size(v) == 1);
    vector_delete(v, NULL);

    return 1;
}

int test_sort_vector() {
    vector_t* v = vector_new();
    
    long a[7] = {2,4,5,8,10,13,19};
    for(int i = 0; i < 7; i++) {
        vector_push(v, (void*)a[i]);
    }
    latte_iterator_t* iterator = vector_get_iterator(v);
    int index = 0;
    while(latte_iterator_has_next(iterator)) {
        void* r = latte_iterator_next(iterator);
        assert((long)r == a[index++]);
    }
    latte_iterator_delete(iterator);
    

    vector_sort(v, int64_cmp);

    long b[7] = {2,4,5,8,10,13,19};
    iterator = vector_get_iterator(v);
    index = 0;
    while(latte_iterator_has_next(iterator)) {
        void* r = latte_iterator_next(iterator);
        assert((long)r == b[index++]);
    }
    latte_iterator_delete(iterator);
    return 1;
}

int test_vector_cmp() {
    vector_t* v1 = vector_new();
    vector_t* v2 = vector_new();
    long a[7] = {2,4,5,8,10,13,19};
    for(int i = 0; i < 7; i++) {
        vector_push(v1, (void*)a[i]);
        vector_push(v2, (void*)a[i]);
    }
    assert(private_vector_cmp(v1, v2, int64_cmp) == 0);
    vector_t* v3 = vector_new();
    long b[6] = {2,4,5,8,10,13};
    for(int i = 0; i < 6; i++) {
        vector_push(v3, (void*)b[i]);
    }
    assert(private_vector_cmp(v1, v3, int64_cmp) > 0);
    vector_t* v4 = vector_new();
    long c[8] = {2,4,5,8,10,13,19, 20};
    for(int i = 0; i < 8; i++) {
        vector_push(v4, (void*)c[i]);
    }
    assert(private_vector_cmp(v1, v4, int64_cmp) < 0);
    vector_t* v5 = vector_new();
    long d[7] = {2,4,5,8,10,13,20};
    for(int i = 0; i < 7; i++) {
        vector_push(v5, (void*)d[i]);
    }
    assert(private_vector_cmp(v1, v5, int64_cmp) < 0);
    return 1;
}

int test_api(void) {
    {
#ifdef LATTE_TEST
        // ..... private
#endif
        test_cond("test vector fuinction", 
            test_vector() == 1);
        test_cond("test vector fuinction", 
            test_sort_vector() == 1);
        test_cond("test vector cmp fuinction", 
            test_vector_cmp() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}