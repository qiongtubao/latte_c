#include "../test/testhelp.h"
#include "../test/testassert.h"

#include "zset.h"
int test_zset() {
    
    return 1;
}
int test_api() {
    {
        //TODO learn usage
        #ifdef LATTE_TEST
            // ..... private method test
        #endif
        test_cond("zset function", 
            test_zset() == 1);
    }
    return 1;
}

int main() {
    test_api();
    return 0;
}