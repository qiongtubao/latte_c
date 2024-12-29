#include "latte_rocksdb_test.h"
#include "../test/testhelp.h"
#include "log/log.h"

int test_api(void) {
    log_init();
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("log add stdout",log_add_stdout(LATTE_LIB, LOG_DEBUG) == 1);
        // test_cond("test_rocksdb_set_get", 
        //     test_rocksdb_set_get() == 1);
        test_cond("test_rocksdb_batch_set_get", 
            test_rocksdb_batch_set_get() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}