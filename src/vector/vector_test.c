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
    vector_delete(v);

    return 1;
}
int long_comparator(void* v1, void* v2) {
    long l1 = (long)v1;
    long l2 = (long)v2;
    return l1 - l2;
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
    

    vector_sort(v, long_comparator);

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

int test_api(void) {
    {
#ifdef LATTE_TEST
        // ..... private
#endif
        test_cond("test vector fuinction", 
            test_vector() == 1);
        test_cond("test vector fuinction", 
            test_sort_vector() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}