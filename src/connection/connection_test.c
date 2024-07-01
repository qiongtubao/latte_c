#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <stdio.h>
#include "ae.h"

int test_connection(void) {
    
    return 1;
}



int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("test_ae function", 
            test_connection() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}