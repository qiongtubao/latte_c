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
    vector* v = vectorCreate();
    assert(v != NULL);
    vectorPush(v, 1);
    assert(vectorGet(v, 0) == 1);
    vectorPush(v, 2);
    assert(vectorGet(v, 1) == 2);

    int a[2] = {1,2};
    Iterator* iterator = vectorGetIterator(v);
    int index = 0;
    while(iteratorHasNext(iterator)) {
        void* r = iteratorNext(iterator);
        assert(r == a[index++]);
    }
    iteratorRelease(iterator);


    assert(vectorSize(v) == 2);
    vectorSet(v, 1, 3);
    assert(vectorGet(v, 1) == 3);

    assert(vectorPop(v) == 3);

    assert(vectorSize(v) == 1);
    vectorRelease(v);

    return 1;
}
int long_comparator(void* v1, void* v2) {
    long l1 = v1;
    long l2 = v2;
    return l1 - l2;
}
int test_sort_vector() {
    vector* v = vectorCreate();
    
    long a[7] = {2,4,5,8,10,13,19};
    for(int i = 0; i < 7; i++) {
        vectorPush(v, a[i]);
    }
    Iterator* iterator = vectorGetIterator(v);
    int index = 0;
    while(iteratorHasNext(iterator)) {
        void* r = iteratorNext(iterator);
        assert(r == a[index++]);
    }
    iteratorRelease(iterator);
    

    vectorSort(v, long_comparator);

    long b[7] = {2,4,5,8,10,13,19};
    iterator = vectorGetIterator(v);
    index = 0;
    while(iteratorHasNext(iterator)) {
        void* r = iteratorNext(iterator);
        assert(r == b[index++]);
    }
    iteratorRelease(iterator);
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