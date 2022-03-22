#include "test/testhelp.h"
#include "test/testassert.h"
#include "zmalloc.h"

int test_zmalloc() {
    //test char
    size_t before = zmalloc_used_memory();
    char* str = zmalloc(100);
    char* c = "hello";
    strcpy(str, c);
    str[strlen(c)] = '\0';
    assert(strcmp(str, c) == 0);
    zfree(str);
    return 1;
}

//test class 
struct zmalloc_test {
    long l;
    double long dl;
};
int test_zmalloc_class() {
    struct zmalloc_test* glass = zmalloc(sizeof(struct zmalloc_test));
    glass->l = 100;
    glass->dl = 200;
    zfree(glass);
    return 1;
}

int test_api() {
    {
        #ifdef LATTE_TEST
            // ..... private method test
        #endif
        test_cond("zmalloc function", 
            test_zmalloc() == 1);
        test_cond("zmalloc class function", 
            test_zmalloc_class() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}