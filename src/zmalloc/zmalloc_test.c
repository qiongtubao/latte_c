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

int test_ztrymalloc() {
    size_t before = zmalloc_used_memory();
    char* str = ztrymalloc(100);
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

int test_zrealloc() {
    size_t before = zmalloc_used_memory();
    char* old = zmalloc(100); 
    assert(old != NULL); 
    char* str = zrealloc(old, 1000);
    assert(old != str);
    char* c = "hello";
    strcpy(str, c);
    str[strlen(c)] = '\0';
    assert(strcmp(str, c) == 0);
    size_t after = zmalloc_used_memory();
    assert(1000 <= after-before);
    zfree(str);
    assert(before == zmalloc_used_memory());
    return 1;
}

int test_ztryrealloc() {
    size_t before = zmalloc_used_memory();
    char* old = zmalloc(100); 
    assert(old != NULL); 
    char* str = ztryrealloc(old, 1000);
    assert(old != str);
    char* c = "hello";
    strcpy(str, c);
    str[strlen(c)] = '\0';
    assert(strcmp(str, c) == 0);
    size_t after = zmalloc_used_memory();
    assert(1000 <= after-before);
    zfree(str);
    assert(before == zmalloc_used_memory());
    return 1;
}

int test_zmalloc_usable() {
    size_t usable = 0;
    void* value = zmalloc_usable(100, &usable);
    assert(usable >= 100);
    zfree(value);
    return 1;
}

int test_ztrymalloc_usable() {
    size_t usable = 0;
    void* value = ztrymalloc_usable(100, &usable);
    assert(usable >= 100);
    zfree(value);
    return 1;
}

int test_zcalloc() {
    size_t before = zmalloc_used_memory();
    void* value = zcalloc(100);
    size_t after = zmalloc_used_memory();
    assert(100 <= (after - before));
    zfree(value);
    return 1;
}

int test_ztrycalloc() {
    size_t before = zmalloc_used_memory();
    void* value = ztrycalloc(100);
    size_t after = zmalloc_used_memory();
    assert(100 <= (after - before));
    zfree(value);
    return 1;
}

int test_zcalloc_usable() {
    size_t usable = 0;
    void* value = zcalloc_usable(100, &usable);
    assert(usable >= 100);
    zfree(value);
    return 1;
}

int test_ztrycalloc_usable() {
    size_t usable = 0;
    void* value = ztrycalloc_usable(100, &usable);
    assert(usable >= 100);
    // assert(NULL == ztrycalloc_usable(0, &usable));
    zfree(value);
    return 1;
}

int test_ztryrealloc_usable() {
    size_t usable = 0;
    void* value = ztryrealloc_usable(NULL, 100, &usable);
    assert(value != NULL);
    value = ztryrealloc_usable(value, 1000, &usable);
    assert(NULL == ztryrealloc_usable(value, 0, &usable));
    return 1;
}

int test_zrealloc_usable() {
    size_t usable = 0;
    void* value = zrealloc_usable(NULL, 100, &usable);
    assert(value != NULL);
    return 1;
}

int test_zfree_usable() {
    void* value = zmalloc(100);
    size_t usable = 0;
    zfree_usable(value, &usable);
    assert(usable >= 100);
    return 1;
}

int test_zstrdup() {
    char* str = zmalloc(100);
    char* c = "hello";
    strcpy(str, c);
    str[strlen(c)] = '\0';
    char* zstr = zstrdup(str);
    assert(zstr != str);
    assert(strncmp(str, zstr, 5) == 0);
    return 1;
}

int test_zmadvise_dontneed() {
    void* value = zmalloc(1000000);
    zmadvise_dontneed(value);
    return 1;
}

int test_zmalloc_get_rss() {
    assert(zmalloc_get_rss() >= 0);
    return 1;
}

int test_zmalloc_get_allocator_info() {
    size_t allocated = 0, active = 0, resident = 0;
    zmalloc_get_allocator_info(&allocated, &active, &resident);
    printf("allocated=%ld, active=%ld, resident=%ld\n", allocated, active, resident);
    return 1;
}

static void test_oom_handler(size_t size) {
    fprintf(stderr, "zmalloc: Out of memory trying to allocate %zu bytes\n",
        size);
    fflush(stderr);
    // abort();
}

int test_zmalloc_set_oom_handler() {
    zmalloc_set_oom_handler(test_oom_handler);
    return 1;
}

int test_set_jemalloc_bg_thread() {
    set_jemalloc_bg_thread(1);
    set_jemalloc_bg_thread(0);
    return 1;
}

int test_jemalloc_purge() {
    jemalloc_purge();
    return 1;
}

int test_zmalloc_get_smap_bytes_by_field() {
    zmalloc_get_smap_bytes_by_field("AnonHugePages:",-1);
    return 1;
}

int test_zmalloc_get_private_dirty() {
    zmalloc_get_private_dirty(-1);
    return 1;
}

int test_zmalloc_get_memory_size() {
    printf("zmalloc_get_memory_size:%ld\n",zmalloc_get_memory_size());
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
        //TODO learn usage
        test_cond("zmalloc_get_allocator_info function",
            test_zmalloc_get_allocator_info() == 1);
        #ifdef LATTE_TEST
            // ..... private method test
        #endif
        test_cond("zmalloc function", 
            test_zmalloc() == 1);
        test_cond("zmalloc class function", 
            test_zmalloc_class() == 1);
        //TODO  learn usage
        test_cond("ztrymalloc function",
            test_ztrymalloc() == 1);
        test_cond("zrealloc function",
            test_zrealloc() == 1);
        test_cond("ztryrealloc function",
            test_ztryrealloc() == 1);
        test_cond("zmalloc_usable function",
            test_zmalloc_usable() == 1);
        test_cond("ztrymalloc_usable function",
            test_ztrymalloc_usable() == 1);
        test_cond("zcalloc_usable function",
            test_zcalloc_usable() == 1);
        test_cond("ztrycalloc_usable function",
            test_ztrycalloc_usable() == 1);
        test_cond("zcalloc function",
            test_zcalloc() == 1);
        test_cond("ztrycalloc function",
            test_ztrycalloc() == 1);
        test_cond("ztryrealloc_usable function",
            test_ztryrealloc_usable() == 1);
        test_cond("zrealloc_usable function",
            test_zrealloc_usable() == 1);
        test_cond("zfree_usbale function",
            test_zfree_usable() == 1);
        test_cond("zstrdup function",   
            test_zstrdup() == 1);
        test_cond("zmadvise_dontneed function",
            test_zmadvise_dontneed() == 1);
        test_cond("zmalloc_get_rss function",
            test_zmalloc_get_rss() == 1);
        test_cond("zmalloc_get_allocator_info function",
            test_zmalloc_get_allocator_info() == 1);
        test_cond("set_jemalloc_bg_thread function",
            test_set_jemalloc_bg_thread() == 1);
        test_cond("zmalloc_set_oom_handler function",
            test_zmalloc_set_oom_handler() == 1);
        test_cond("jemalloc_purge function",
            test_jemalloc_purge() == 1);
        test_cond("zmalloc_get_smap_bytes_by_field function", 
            test_zmalloc_get_smap_bytes_by_field() == 1);
        test_cond("zmalloc_get_private_dirty function", 
            test_zmalloc_get_private_dirty() == 1);
        test_cond("zmalloc_get_memory_size function",
            test_zmalloc_get_memory_size() == 1);
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