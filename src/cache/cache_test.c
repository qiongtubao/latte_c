


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
    latte_pair_t* pair = lru_cache_get(cache, (void*)2L);
    assert((long)latte_pair_key(pair) == 2L);
    assert((long)latte_pair_value(pair) == 2L);

    assert(lru_cache_put(cache, (void*)3L, (void*)3L) == 1);

    
    latte_iterator_t* iterator = lru_cache_get_iterator(cache);
    long key0s[3] = {3,2,1};
    long val0s[3] = {3,2,2};
    int i = 0;
    while(latte_iterator_has_next(iterator)) {
        latte_pair_t* p = latte_iterator_next(iterator);
        // printf("p: %p, index:%d key:%ld value:%ld\n", p, i, (long)latte_pair_key(p), (long)latte_pair_value(p));
        assert(key0s[i] == (long)latte_pair_key(p));
        assert(val0s[i] == (long)latte_pair_value(p));
        i++;
    }
    assert(i == 3);
    latte_iterator_delete(iterator);

    lru_cache_put(cache, (void*)2L, (void*)3L);

    iterator = lru_cache_get_iterator(cache);
    long key1s[3] = {2,3,1};
    long val1s[3] = {3,3,2};
    i = 0;
    while(latte_iterator_has_next(iterator)) {
        latte_pair_t* p = latte_iterator_next(iterator);
        // printf("p: %p, index:%d key:%ld value:%ld\n", p, i, (long)latte_pair_key(p), (long)latte_pair_value(p));
        assert(key1s[i] == (long)latte_pair_key(p));
        assert(val1s[i] == (long)latte_pair_value(p));
        i++;
    }
    assert(i == 3);
    latte_iterator_delete(iterator);



    lru_cache_pop(cache);
    iterator = lru_cache_get_iterator(cache);
    long key2s[2] = {2,3};
    long val2s[2] = {3,3};
    i = 0;
    while(latte_iterator_has_next(iterator)) {
        latte_pair_t* p = latte_iterator_next(iterator);
        assert(key1s[i] == (long)latte_pair_key(p));
        assert(val1s[i] == (long)latte_pair_value(p));
        i++;
    }
    assert(i == 2);
    latte_iterator_delete(iterator);

    lru_cache_delete(cache);
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