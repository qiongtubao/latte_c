#include "../test/testhelp.h"
#include "../test/testassert.h"
#include "async_io.h"
#include "debug/latte_debug.h"
#include "utils/utils.h"
#include "log/log.h"


int test_async_io_net_read() {
    return 1;
}



int test_api(void) {
    log_init();
    assert(log_add_stdout(LATTE_LIB, LOG_DEBUG) == 1);
    assert(log_add_stdout(LATTE_LIB, LOG_DEBUG) == 1);
    {
        
        test_cond("async io net read", 
            test_async_io_net_read() == 1);
        
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}