
#include "../test/testhelp.h"
#include "../test/testassert.h"

#include "sds/sds.h"
#include "list.h"

int test_list_iterator() {
    list* l = listCreate();
    l->free = sdsfree;
    listAddNodeTail(l, sdsnew("hello"));
    listAddNodeTail(l, sdsnew("world"));
    Iterator* iter = listGetLatteIterator(l, 1);
    int i = 0;
    while (iteratorHasNext(iter)) {
        iteratorNext(iter);
        i++;
    }
    assert(i == 2);
    iteratorRelease(iter);
    return 1;
}

int test_list() {
    list* l = listCreate();
    
    listAddNodeHead(l, 0);
    listAddNodeTail(l, 1);
    listAddNodeTail(l, 2);
    listAddNodeTail(l, 3);
    Iterator* iter = listGetLatteIterator(l, 0);
    int a[4] = {0,1,2,3};
    int i = 0;
    while (iteratorHasNext(iter)) {
        int v = iteratorNext(iter);
        assert(a[i] == v);
        i++;
    }
    iteratorRelease(iter);
    int b[4] = {1,0,2,3};
    i = 0;
    listMoveHead(l, l->head->next);
    for(int i = 0; i < 4; i++) {
        assert(listNodeValue(listIndex(l, i)) == b[i]);
    }

    int r = listPop(l);
    assert(r == 1);
    assert(listLength(l) == 3);

    listRelease(l);
    return 1;
}

int test_api(void) {

    {
        
        test_cond("list iterator test", 
            test_list_iterator() == 1);
        test_cond("list function test", 
            test_list() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}