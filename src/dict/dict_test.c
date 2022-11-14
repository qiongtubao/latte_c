


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
    return dictGenCaseHashFunction((unsigned char*)key, strlen((char*)key));
}

int testCompareCallback(void *privdata, const void *key1, const void *key2) {
    int l1,l2;
    DICT_NOTUSED(privdata);
    l1 = strlen((char*) key1);
    l2 = strlen((char*) key2);
    if(l1 != l2) return 0;
    return memcmp(key1, key2,l1) == 0;
}

void testFreeCallback(void* privdata, void* val) {
    DICT_NOTUSED(privdata);
    printf("delete :%p\n", val);
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
static dictType testDict = {
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


int test_dictCreate() {
    dict *dict = dictCreate(&testDict);
    assert((long)dictSize(dict) == 0);
    dictRelease(dict);
    return 1;
}

int test_dictExpand() {
    dict *dict = dictCreate(&testDict);
    assert(DICTHT_SIZE((dict)->ht_size_exp[0]) == 0);
    assert(DICTHT_SIZE((dict)->ht_size_exp[1]) == 0);
    dictExpand(dict, 100);
    //2 ^ x
    assert(DICTHT_SIZE((dict)->ht_size_exp[0]) == 128);
    assert(DICTHT_SIZE((dict)->ht_size_exp[1])== 0);
    dictRelease(dict);
    return 1;
}

int test_dictAdd() {
    dict *dict = dictCreate(&testDict);
    long count = 5000;
    //dict add long long
    // for (int j = 0; j < count; j++) {
    //     dictEntry* de = dictAddOrFind(dict, stringFromLongLong(j));
    //     dictSetSignedIntegerVal(de, (long long)j);
    // }
    // assert((long)dictSize(dict) == count);
    dictRelease(dict);
    return 1;
}

int test_dictAdd_speed() {
    dict *dict = dictCreate(&testDict);
    long count = 5000;
    long long start, elapsed;
    start_benchmark();
    //dict add long long
    // for (int j = 0; j < count; j++) {
    //     dictEntry* de = dictAddOrFind(dict, stringFromLongLong(j));
    //     dictSetSignedIntegerVal(de, (long long)j);
    // }
    // assert((long)dictSize(dict) == count);
    end_benchmark("\nInserting");
    dictRelease(dict);
    return 1;
}

int test_dictAddRaw() {
    dict* dict = dictCreate(&testDict);
    char* key = stringFromLongLong(100);
    dictEntry* entry = dictAddRaw(dict, key, NULL);
    assert(entry != NULL);
    assert(dictAddRaw(dict, key, NULL) == NULL);
    dictRelease(dict);
    return 1;
}

int test_dictAddOrFind() {
    dict* dict = dictCreate(&testDict);
    char* key = stringFromLongLong(100);
    dictEntry* create_entry = dictAddOrFind(dict, key);
    assert(create_entry != NULL);
    dictEntry* find_entry = dictAddOrFind(dict, key);
    assert(find_entry != NULL);
    return 1;
}


int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("dictCreate function", 
            test_dictCreate() == 1);
        test_cond("dictAdd function",
            test_dictAdd() == 1);
        test_cond("dictExpand function",
            test_dictExpand() == 1);
        test_cond("dictAddRaw function",
            test_dictAddRaw() == 1);
        test_cond("dictAddOrFind function",
            test_dictAddOrFind() == 1);
        test_cond("dictAdd speed",
            test_dictAdd_speed() == 1);
        
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}