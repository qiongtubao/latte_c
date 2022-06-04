#include "test/testhelp.h"
#include "test/testassert.h"
#include <stdio.h>
#include "net.h"

int test_server(void) {
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("server function", 
            test_server() == 1);
        
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}