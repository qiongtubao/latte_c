

#include "test/testhelp.h"
#include "test/testassert.h"
#include "log/log.h"
#include "latte_debug.h"
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
        log_module_init();
        assert(log_add_stdout(LATTE_LIB, LL_DEBUG) == 1);
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