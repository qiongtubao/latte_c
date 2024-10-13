


#include "test/testhelp.h"
#include "test/testassert.h"

#include <stdio.h>
#include "dict.h"
#include "zmalloc/zmalloc.h"
#include <sys/time.h>

long long timeInMilliseconds(void) {
    struct timeval tv;

    gettimeofday(&tv,NULL);
    return (((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}

uint64_t testHashCallback(const void *key) {
    return dict_gen_case_hash_function((unsigned char*)key, strlen((char*)key));
}

int testCompareCallback(dict_t*privdata, const void *key1, const void *key2) {
    int l1,l2;
    DICT_NOTUSED(privdata);
    l1 = strlen((char*) key1);
    l2 = strlen((char*) key2);
    if(l1 != l2) return 0;
    return memcmp(key1, key2,l1) == 0;
}

void testFreeCallback(dict_t* privdata, void* val) {
    DICT_NOTUSED(privdata);
    // printf("delete :%p\n", val);
    if(val != NULL) {
        zfree(val);
    }
}

/**
 * @brief 
 * 
 * @return long long
 *  
 */
static dict_func_t testDict = {
    testHashCallback,
    NULL,
    NULL,
    testCompareCallback,
    testFreeCallback,
    NULL,
    NULL
};

char* stringFromLongLong(long long value) {
    char buf[32];
    int len;
    char *s;
    len = sprintf(buf, "%lld", value);
    s = zmalloc(len + 1);
    memcpy(s, buf, len);
    s[len] = '\0';
    return s;
}

#define start_benchmark() do { \
    start = timeInMilliseconds(); \
} while(0)

#define end_benchmark(msg) do { \
    elapsed = timeInMilliseconds()-start; \
    printf(msg ": %ld items in %lld ms\n", count, elapsed); \
} while(0)


int test_dict_new() {
    dict_t*dict= dict_new(&testDict);
    assert((long)dict_size(dict) == 0);
    dict_delete(dict);
    return 1;
}

int test_dict_expand() {
    dict_t*dict = dict_new(&testDict);
    assert(DICTHT_SIZE((dict)->ht_size_exp[0]) == 0);
    assert(DICTHT_SIZE((dict)->ht_size_exp[1]) == 0);
    dict_expand(dict, 100);
    //2 ^ x
    assert(DICTHT_SIZE((dict)->ht_size_exp[0]) == 128);
    assert(DICTHT_SIZE((dict)->ht_size_exp[1])== 0);
    dict_delete(dict);
    return 1;
}

int test_dict_add() {
    dict_t* dict= dict_new(&testDict);
    // long count = 5000;
    //dict_tadd long long
    // for (int j = 0; j < count; j++) {
    //     dict_entry_t* de = dict_add_or_find(dict, stringFromLongLong(j));
    //     dictSetSignedIntegerVal(de, (long long)j);
    // }
    // assert((long)dict_size(dict) == count);
    dict_delete(dict);
    return 1;
}

int test_dict_add_speed() {
    dict_t*dict = dict_new(&testDict);
    long count = 5000;
    long long start, elapsed;
    start_benchmark();
    //dict_tadd long long
    // for (int j = 0; j < count; j++) {
    //     dict_entry_t* de = dict_add_or_find(dict, stringFromLongLong(j));
    //     dictSetSignedIntegerVal(de, (long long)j);
    // }
    // assert((long)dict_size(dict) == count);
    end_benchmark("\nInserting");
    dict_delete(dict);
    return 1;
}

int test_dict_add_raw() {
    dict_t* dict= dict_new(&testDict);
    char* key = stringFromLongLong(100);
    dict_entry_t* entry = dict_add_raw(dict, key, NULL);
    assert(entry != NULL);
    assert(dict_add_raw(dict, key, NULL) == NULL);
    dict_delete(dict);
    return 1;
}

int test_dict_add_or_find() {
    dict_t* dict= dict_new(&testDict);
    char* key = stringFromLongLong(100);
    dict_entry_t* create_entry = dict_add_or_find(dict, key);
    assert(create_entry != NULL);
    dict_entry_t* find_entry = dict_add_or_find(dict, key);
    assert(find_entry != NULL);
    dict_delete(dict);
    return 1;
}


int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("dict_new function", 
            test_dict_new() == 1);
        test_cond("dict_add function",
            test_dict_add() == 1);
        test_cond("dict_expand function",
            test_dict_expand() == 1);
        test_cond("dict_add_raw function",
            test_dict_add_raw() == 1);
        test_cond("dict_add_or_find function",
            test_dict_add_or_find() == 1);
        test_cond("dict_add speed",
            test_dict_add_speed() == 1);
        
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}