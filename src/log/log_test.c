
#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <stdio.h>
#include "log.h"

int test_log(void) {
    assert(remove("./test.log") == 0);
    setLogFile("./test.log");
    setLogLevel(LL_DEBUG);
    //type 5
    Log(LL_DEBUG, "hello");
    FILE *file = fopen("./test.log", "r");
    assert(file != NULL);
    char line[100];
    assert(fgets(line, sizeof(line), file) > 0);
    assert(strstr(line, "hello") != NULL);

    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        assert(test_log() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}