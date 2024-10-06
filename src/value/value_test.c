#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <unistd.h>
#include "value.h"
int test_value() {
    value* v = valueCreate();
    valueSetInt64(v, 1000L);
    assert(1000L == valueGetInt64(v));
    valueSetUint64(v, 30000ULL);
    assert(30000ULL == valueGetUint64(v));
    valueSetBool(v, true);
    assert(true == valueGetBool(v));
    valueSetLongDouble(v, 1.11);
    assert(1.11 == valueGetLongDouble(v));
   
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