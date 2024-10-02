#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <unistd.h>
#include "value.h"
int test_value() {
    value* v = valueCreate();
    valueSetLong(v, 1000L);
    assert(1000L == valueGetLong(v));
    valueSetBool(v, true);
    assert(true == valueGetBool(v));
    valueSetDouble(v, 1.11);
    assert(1.11 == valueGetDouble(v));
   
    valueSetSds(v, sdsnew("hello"));
    assert(strcmp("hello", valueGetSds(v)) == 0);
    dictType t = {
        
    };
    dict* d = dictCreate(&t);
    valueSetDict(v, d);
    assert(d == valueGetDict(v));

    vector* ve = vectorCreate();
    valueSetArray(v, ve);
    assert(ve == valueGetArray(v));
    valueRelease(v);
    return 1;
}
int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("about tree function", 
            test_value() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}