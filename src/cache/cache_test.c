


#include "test/testhelp.h"
#include "test/testassert.h"

#include "lru_cache.h"

uint64_t test_long_hash_callback(const void *key) {
    return (uint64_t)key;
}

int test_long_compare_callback(dict_t*privdata, const void *key1, const void *key2) {
    long l1,l2;
    DICT_NOTUSED(privdata);
    l1 = (long)key1;
    l2 = (long)key2;
    return l1 - l2;
}

lru_cache_func_t lru_cache_test_func = {
    .super.hashFunction = test_long_hash_callback,
    .super.keyCompare = test_long_compare_callback
};

int test_lru_cache() {
    lru_cache_t* cache = lru_cache_new(&lru_cache_test_func);
    assert(lru_cache_put(cache, (void*)1L, (void*)1L) == 1);
    assert(list_length(cache->list) == 1);
    assert(dict_size(cache->searcher) == 1);
    assert(lru_cache_put(cache, (void*)1L, (void*)2L) == 0);
    assert(list_length(cache->list) == 1);
    assert(dict_size(cache->searcher) == 1);
    assert(lru_cache_put(cache, (void*)2L, (void*)2L) == 1);
    assert(list_length(cache->list) == 2);
    assert(dict_size(cache->searcher) == 2);
    assert((long)lru_cache_get(cache, 2L) == 2L);
    
    latte_iterator_t* iterator = lru_cache_get_iterator(cache);
    while(latte_iterator_has_next(iterator)) {
        latte_pair_t* pair = latte_iterator_next(iterator);
        printf("key: %d ,value: %d\n", latte_pair_key(pair), latte_pair_value(pair));
    }

    list_iterator_delete(iterator);
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        
        test_cond("lru test function",
            test_lru_cache() == 1);
        
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}