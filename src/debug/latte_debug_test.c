

#include "test/testhelp.h"
#include "test/testassert.h"
#include "latte_debug.h"

/* NOTE: latte_assert/latte_panic abort the process by design, so these two
 * helpers never return. This test file predates that and has always been
 * non-runnable as written. */
int test_assert() {
    latte_assert(0);
    return 1;
}

int test_panic() {
    latte_panic("test");
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("test_assert function", 
            test_assert() == 1);
        test_cond("test_panic function", 
            test_panic() == 1);
        
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}
