#include "test/testhelp.h"
#include "test/testassert.h"

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}