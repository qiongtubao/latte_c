#include "test/testhelp.h"
#include "test/testassert.h"
#include <stdio.h>
#include "dict.h"

int test_dictCreate() {
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("dictCreate function", 
            test_dictCreate() == 1);
        
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}