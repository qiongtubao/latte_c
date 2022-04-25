#include "test/testhelp.h"
#include "test/testassert.h"

#include <stdio.h>
#include "dict.h"
#include "zmalloc.h"

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
    zfree(val);
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
    
    dict *dict = dictCreate(&testDict, NULL);

    long count = 5000;
    long long start, elapsed;
    start_benchmark();
    for (int j = 0; j < count; j++) {
        int retval = dictAdd(dict,  stringFromLongLong(j), (void*)j);
        assert(retval == DICT_OK);
    }
    end_benchmark("Inserting");
    assert((long)dictSize(dict) == count);


    dictRelease(dict);
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("dictCreate function", 
            test_dictCreate() == 1);
        
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}