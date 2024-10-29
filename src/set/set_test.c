#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <unistd.h>
#include "set.h"
#include "avl_set.h"
#include "hash_set.h"
#include "sds/sds.h"

typedef sds_t (* getKey)(void*);
int test_set_base(set_t* s1, getKey getNodeKey, int order) {
    sds_t key = sds_new("key");
    sds_t key1 = sds_new("key1");
    sds_t key2 = sds_new("key2");

    
    assert(set_contains(s1, key) == 0);
    assert(set_size(s1) == 0);
    assert(set_add(s1, key) == 1);
    assert(set_contains(s1, key) == 1);
    assert(set_add(s1, key) == 0);
    assert(set_size(s1) == 1);
    assert(set_remove(s1, key) == 1);
    assert(set_remove(s1, key) == 0);
    assert(set_size(s1) == 0);

    assert(set_add(s1, key) == 1);
    assert(set_add(s1, key1) == 1);
    assert(set_add(s1, key2) == 1);
    assert(set_size(s1) == 3);

    latte_iterator_t* iterator = set_get_iterator(s1);
    int i = 0;
    sds_t keys[3] = {key, key1, key2};
    while (latte_iterator_has_next(iterator)) {
        void* node = latte_iterator_next(iterator);
        if (order) {
            assert(sds_cmp(getNodeKey(node), keys[i]) == 0);
        } else {
            assert(sds_cmp(getNodeKey(node), keys[0]) == 0 ||
                sds_cmp(getNodeKey(node), keys[1]) == 0 ||
                sds_cmp(getNodeKey(node), keys[2]) == 0 );
        }
        i++;
    }
    assert(i == 3);
    latte_iterator_delete(iterator);

    sds_delete(key);
    sds_delete(key1);
    sds_delete(key2);
    set_delete(s1);
    return 1;
}

sds_t get_hash_node_key(void* node) {
    latte_pair_t* n = (latte_pair_t*)node;
    return (sds_t)n->key;
}
int test_hash_set_api() {
    set_t* s = hash_set_new(&sds_hash_set_dict_func);
    return test_set_base(s, get_hash_node_key, 0);
}

int test_hash_set() {
    assert(test_hash_set_api() == 1);
    return 1;
}

sds_t get_avl_node(void* n) {
    avl_node_t* node = (avl_node_t*)n;
    return node->key;
}

int test_avl_set_set_api() {
    set_t* s1 = avl_set_new(&avl_set_sds_func);
    return test_set_base(s1, get_avl_node, 1);
}

int test_avl_set() {
    assert(test_avl_set_set_api() == 1);
    
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("about set function", 
            test_hash_set() == 1);
        test_cond("about avlSet function",
            test_avl_set() == 1);

    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}