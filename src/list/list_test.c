#include "test/testhelp.h"
#include "test/testassert.h"
#include <stdio.h>
#include "list.h"
#include <stdarg.h>

list* initList(list* list, int size, ...) {
    va_list ap;
    void* v;
    va_start(ap, size);
    for( int i = 0; i < size; i++) {
        v = va_arg(ap, void*);
        list = listInsertNode(list, i, v);
    }
    va_end(ap);
    return list;
}

int assertList(list* list, int size, ...) {
    va_list ap;
    void* v;
    va_start(ap, size);
    for( int i = 0; i < size; i++) {
        v = va_arg(ap, void*);
        void* value = list->getNodeValue(listIndex(list, i));
        assert(v == value);
    }
    va_end(ap);
    return 1;
}

int test_createDlinkedList() {
    long size;
    long value;
    list* list = createDlinkedList();
    list = initList(list,3,1,2,3);
    size = listSize(list);
    assert(size == 3);
    assert(assertList(list,3,1,2,3) == 1);
    list = listInsertNode(list, 1, 0);
    assert(assertList(list,4,1,0,2,3) == 1);
    list = listInsertNode(list, -1, 0);
    assert(assertList(list,5,1,0,2,3,0) == 1);
    list = listInsertNode(list, -2, 1);
    assert(assertList(list,5,1,0,2,3,1,0) == 1);
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("dlinkedList add function", 
            test_createDlinkedList() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}