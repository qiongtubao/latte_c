#include "test/testhelp.h"
#include "test/testassert.h"
#include <stdio.h>
#include "sds.h"

int test_sdsnew(void) {
    sds s = sdsnew("hello");
    assert(strcmp(s, "hello") == 0);
    sdsfree(s);
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // .....
        #endif
        test_cond("sdsnew function", 
            test_sdsnew() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}