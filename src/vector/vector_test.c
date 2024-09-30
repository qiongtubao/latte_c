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
    Iterator* iter = vectorGetIterator(v);

    return 1;
}

int test_api(void) {
    {
#ifdef LATTE_TEST
        // ..... private
#endif
        test_cond("test vector fuinction", 
            test_vector() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}