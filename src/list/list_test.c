
#include "../test/testhelp.h"
#include "../test/testassert.h"

#include "sds/sds.h"
#include "list.h"

int test_list() {
    list* l = listCreate();
    l->free = sdsfree;
    listAddNodeTail(l, sdsnew("hello"));
    listAddNodeTail(l, sdsnew("world"));
    Iterator* iter = listGetLatteIterator(l, 0);
    int i = 0;
    while (iteratorHasNext(iter)) {
        iteratorNext(iter);
        i++;
    }
    assert(i == 2);
    iteratorRelease(iter);
    return 1;
}

int test_api(void) {

    {
        
        test_cond("list iterator test", 
            test_list() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}