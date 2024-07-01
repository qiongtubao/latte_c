#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <stdio.h>
#include "anet.h"

int test_anet(void) {
    char neterr[128];
    int fd = anetTcpServer(neterr, 6379, "127.0.0.1", 511);
    return 1;
}



int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("test_anet function", 
            test_anet() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}