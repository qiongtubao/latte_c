
#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <stdio.h>
#include "log.h"
#include <unistd.h>
#include <fcntl.h>

// int test_log(void) {
//     assert(remove("./test.log") == 0);
//     setLogFile("./test.log");
//     setLogLevel(LL_INFO);
//     //type 5
//     Log(LL_DEBUG, "hello1");
//     FILE *file = fopen("./test.log", "r");
//     assert(file != NULL);
//     char line[100];
//     assert(fgets(line, sizeof(line), file) > 0);
//     assert(strstr(line, "hello1") != NULL);

//     return 1;
// }
int test_set_log() {
    initLogger();
    if (access("./test.log", F_OK) != -1) {
        assert(remove("./test.log") == 0);
    }

    log_debug("test", "test %s", "word"); 
    
    assert(log_add_stdout("test", LOG_DEBUG) == 1);
    assert(log_add_file("test", "./test.log", LOG_INFO) == 1);
    log_debug("test", "hello %s", "word");
    assert(access("./test.log", F_OK) == -1);
    char line[100];
    FILE *file = fopen("./test.log", "r");
    
    // fclose(file);
    
    log_error("test", "hello1 %s", "word");
    file = fopen("./test.log", "r");
    assert(fgets(line, sizeof(line), file) > 0);
    assert(strstr(line, "hello1 word") != NULL);
    fclose(file);
    assert(remove("./test.log") == 0);
    return 1;
}

int test_api(void) {

    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("log test", 
            test_set_log() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}