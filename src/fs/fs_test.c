#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <stdio.h>
#include "dir.h"
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include "file.h"
int check_dir_exists(char* dirname) {
    int exists = access(dirname, F_OK);

    if (exists == 0) {
        //存在
        return 1;
    } else if (exists == -1) {
        if (errno == ENOENT) {
            //不存在
            return 0;
        } else {
            //异常
            printf("[check dir exists] %s", strerror(errno));
            return -1;
        }
    }

}
int test_create_dir() {
    assert(check_dir_exists("test_create_dir") == 0);
    Error* error = createDir("test_create_dir");
    assert(isOk(error));
    assert(check_dir_exists("test_create_dir") == 1);
    return 1;
}


int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("create dir function", 
            test_create_dir() == 1);
        
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}