#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <stdio.h>
#include "utils.h"

int test_ll2string(void) {
    //type 5
    
    return 1;
}



int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("ll2string function", 
            test_ll2string() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}