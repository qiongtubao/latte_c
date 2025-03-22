//
// Created by dong on 23-5-22.
//

#include "test/testhelp.h"
#include "test/testassert.h"

#include <stdio.h>
#include "zmalloc/zmalloc.h"
#include "quicklist.h"


int test_quicklist() {
    struct quicklist_t* list = quicklist_new();
//    quicklistPushHead(list, "hello", 6);
//    quicklistRepr(list , 1);
    return 1;
}

int test_api(void) {
    {
#ifdef LATTE_TEST
        // ..... private
#endif
        test_cond("quicklist function",
                  test_quicklist() == 1);

    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}