#include "../test/testhelp.h"
#include "../test/testassert.h"
#include "latte_list.h"
#include "sds/sds.h"


int test_create_list() {
    struct latte_list* list = createList();
    assert(latteListLength(list) == 0);
    sds a = sdsnew("a");
    list = latteListAddNodeHead(list, a);
    assert(list != NULL);
    assert(latteListLength(list) == 1);
    sds b = sdsnew("b");
    list = latteListAddNodeHead(list, b);
    assert(latteListLength(list) == 2);

    assert( latteListIndex(list, 0) == b);
    assert( latteListIndex(list, 1) == a);
    
    assert(latteListDelByIndex(list, 0) == b);
    assert(latteListLength(list) == 1);
    assert( latteListIndex(list, 0) == a);
    return 1;
}

int test_api() {
    {
        #ifdef LATTE_TEST
            // ..... private method test
        #endif
        test_cond("create_list function", 
            test_create_list() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}