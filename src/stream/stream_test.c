
#include "../test/testhelp.h"
#include "../test/testassert.h"

#include "stream.h"
int test_stream() {
    
    return 1;
}
int test_api() {
    {
        //TODO learn usage
        #ifdef LATTE_TEST
            // ..... private method test
        #endif
        test_cond("stream function", 
            test_stream() == 1);
    }
    return 1;
}

int main() {
    test_api();
    return 0;
}