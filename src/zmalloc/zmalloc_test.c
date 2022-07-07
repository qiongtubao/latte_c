#include "../test/testhelp.h"
#include "../test/testassert.h"
#include "zmalloc.h"



int test_zmalloc() {
    //test char
    size_t before = zmalloc_used_memory();
    char* str = zmalloc(100);
    char* c = "hello";
    strcpy(str, c);
    str[strlen(c)] = '\0';
    assert(strcmp(str, c) == 0);
    size_t after = zmalloc_used_memory();
    assert(100 <= after-before);
    zfree(str);
    assert(before == zmalloc_used_memory());
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

/* this method was added to jemalloc in order to help us understand which
 * pointers are worthwhile moving and which aren't */
#ifdef HAVE_DEFRAG
int je_get_defrag_hint(void* ptr);
int test_je_get_defrag_hint() {
    int len = 100000;
    struct zmalloc_test* array[len];
    for(int i = 0; i < len; i++) {
        array[i] = zmalloc(sizeof(struct zmalloc_test));
        array[i]->l = i;
        array[i]->dl = i;
    }
    // struct zmalloc_test* will_frees[len/2];
    for(int i = 0; i < len/2; i++) {
        struct zmalloc_test* will_free = array[i*2];
        // will_frees[i] = will_free;
        zfree(will_free);
        array[i*2] = NULL;
    }
    // printf("je_get_defrag_hint %d\n",je_get_defrag_hint(array[len - 1]));
    struct zmalloc_test* current_test = array[len - 1];
    assert(je_get_defrag_hint(current_test) == 1);
    printf("\naddress: %lld, value: %ld, size:%ld\n", (long long)current_test, current_test->l, zmalloc_size(current_test));
    void* ptr = current_test;
    size_t size = zmalloc_size(ptr);
    struct zmalloc_test* newptr = zmalloc_no_tcache(size);
    memcpy(newptr, ptr, size);
    zfree_no_tcache(ptr);
    // zfree_no_tcache(ptr);
    printf("\naddress: %lld, value: %ld, size:%ld\n", (long long)current_test, current_test->l, zmalloc_size(current_test));
    printf("\nnew_address: %lld, value: %ld, size:%ld\n", (long long)newptr, newptr->l, zmalloc_size(newptr));
    struct zmalloc_test* new_array[len];
    for(int i = 0; i < len;i++) {
        new_array[i] = zmalloc(sizeof(struct zmalloc_test));
        new_array[i]->l = i;
        new_array[i]->dl = i;
        // assert(new_array[i] != ptr);
    }
    struct zmalloc_test* new_array_ptr = new_array[len-1];
    printf("\nnew1_address: %lld, value: %ld, size:%ld\n", (long long)new_array_ptr, new_array_ptr->l, zmalloc_size(new_array_ptr));
    return 1;
}
#else /* HAVE_DEFRAG */
#endif

int test_api() {
    {
        #ifdef LATTE_TEST
            // ..... private method test
        #endif
        test_cond("zmalloc function", 
            test_zmalloc() == 1);
        test_cond("zmalloc class function", 
            test_zmalloc_class() == 1);
        #ifdef HAVE_DEFRAG
            test_cond("je_get_defrag_hint function",
                test_je_get_defrag_hint() == 1);
        #endif
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}