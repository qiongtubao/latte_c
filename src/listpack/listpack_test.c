

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
    unsigned char* newp = NULL;
    lp = list_pack_insert_string(lp, "hello", 5, lp + 6, LIST_PACK_BEFORE, &newp);
    assert(newp != NULL);
    assert(newp == lp + 6);
    // log_debug("test","%d\n",list_pack_bytes(lp));
    assert(list_pack_bytes(lp) == 14);
    lp = list_pack_insert_integer(lp, 100, lp + 6, LIST_PACK_AFTER, &newp);
    assert(newp != NULL);
    assert(newp == lp + 13);

    assert(list_pack_length(lp) == 2);
    assert(list_pack_bytes(lp) == 16);

    lp = list_pack_append_integer(lp, 200);
    assert(list_pack_length(lp) == 3);
    assert(list_pack_bytes(lp) == 19);
    lp = list_pack_append_string(lp, "world", 5);
    assert(list_pack_length(lp) == 4);
    assert(list_pack_bytes(lp) == 26);
    lp =list_pack_prepend_string(lp, "test", 4);
    assert(list_pack_length(lp) == 5);
    assert(list_pack_bytes(lp) == 32);

    lp = list_pack_prepend_integer(lp, 300);
    assert(list_pack_length(lp) == 6);
    assert(list_pack_bytes(lp) == 35);

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