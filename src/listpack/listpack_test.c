

#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <stdio.h>
#include "listpack.h"
#include <unistd.h>
#include <fcntl.h>
#include "log/log.h"

int test_list_pack(void) {
    list_pack_t* lp = list_pack_new(1024);
    assert(lp != NULL);
    lp = list_pack_shrink_to_fit(lp);
    list_pack_delete(lp);
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        log_module_init();
        assert(log_add_stdout("test", LOG_DEBUG) == 1);
        assert(log_add_file("test", "./test.log", LOG_INFO) == 1);
        test_cond("test_list_pack test", 
            test_list_pack() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}