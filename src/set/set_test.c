#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <unistd.h>
#include "set.h"
#include "avl_set.h"
#include "hash_set.h"
#include "sds/sds.h"
#include "set/int_set.h"
#include "endianconv/endianconv.h"
#include <sys/time.h>

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

static int_set_t *create_int_set(int bits, int size) {
    uint64_t mask = (1<<bits)-1;
    uint64_t value;
    int_set_t *is = int_set_new();

    for (int i = 0; i < size; i++) {
        if (bits > 32) {
            value = (rand()*rand()) & mask;
        } else {
            value = rand() & mask;
        }
        is = int_set_add(is,value,NULL);
    }
    return is;
}

static int checkConsistency(int_set_t *is) {
    for (uint32_t i = 0; i < (intrev32ifbe(is->length)-1); i++) {
        uint32_t encoding = intrev32ifbe(is->encoding);

        if (encoding == INTSET_ENC_INT16) {
            int16_t *i16 = (int16_t*)is->contents;
            assert(i16[i] < i16[i+1]);
        } else if (encoding == INTSET_ENC_INT32) {
            int32_t *i32 = (int32_t*)is->contents;
            assert(i32[i] < i32[i+1]);
        } else {
            int64_t *i64 = (int64_t*)is->contents;
            assert(i64[i] < i64[i+1]);
        }
    }
}

static void ok(void) {
    printf("OK\n");
}

static long long usec(void) {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return (((long long)tv.tv_sec)*1000000)+tv.tv_usec;
}

#define UNUSED(x) (void)(x)
int int_set_test(int argc, char** argv, int accurate) {
    uint8_t success;
    int i;
    int_set_t *is;
    srand(time(NULL));

    UNUSED(argc);
    UNUSED(argv);
    UNUSED(accurate);

    printf("Value encodings: "); {
        assert(_int_set_value_encoding(-32768) == INTSET_ENC_INT16);
        assert(_int_set_value_encoding(+32767) == INTSET_ENC_INT16);
        assert(_int_set_value_encoding(-32769) == INTSET_ENC_INT32);
        assert(_int_set_value_encoding(+32768) == INTSET_ENC_INT32);
        assert(_int_set_value_encoding(-2147483648) == INTSET_ENC_INT32);
        assert(_int_set_value_encoding(+2147483647) == INTSET_ENC_INT32);
        assert(_int_set_value_encoding(-2147483649) == INTSET_ENC_INT64);
        assert(_int_set_value_encoding(+2147483648) == INTSET_ENC_INT64);
        assert(_int_set_value_encoding(-9223372036854775808ull) ==
                    INTSET_ENC_INT64);
        assert(_int_set_value_encoding(+9223372036854775807ull) ==
                    INTSET_ENC_INT64);
        ok();
    }

    printf("Basic adding: "); {
        is = int_set_new();
        is = int_set_add(is,5,&success); assert(success);
        is = int_set_add(is,6,&success); assert(success);
        is = int_set_add(is,4,&success); assert(success);
        is = int_set_add(is,4,&success); assert(!success);
        ok();
        zfree(is);
    }

    printf("Large number of random adds: "); {
        uint32_t inserts = 0;
        is = int_set_new();
        for (i = 0; i < 1024; i++) {
            is = int_set_add(is,rand()%0x800,&success);
            if (success) inserts++;
        }
        assert(intrev32ifbe(is->length) == inserts);
        checkConsistency(is);
        ok();
        zfree(is);
    }

    printf("Upgrade from int16 to int32: "); {
        is = int_set_new();
        is = int_set_add(is,32,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT16);
        is = int_set_add(is,65535,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT32);
        assert(int_set_find(is,32));
        assert(int_set_find(is,65535));
        checkConsistency(is);
        zfree(is);

        is = int_set_new();
        is = int_set_add(is,32,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT16);
        is = int_set_add(is,-65535,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT32);
        assert(int_set_find(is,32));
        assert(int_set_find(is,-65535));
        checkConsistency(is);
        ok();
        zfree(is);
    }

    printf("Upgrade from int16 to int64: "); {
        is = int_set_new();
        is = int_set_add(is,32,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT16);
        is = int_set_add(is,4294967295,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT64);
        assert(int_set_find(is,32));
        assert(int_set_find(is,4294967295));
        checkConsistency(is);
        zfree(is);

        is = int_set_new();
        is = int_set_add(is,32,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT16);
        is = int_set_add(is,-4294967295,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT64);
        assert(int_set_find(is,32));
        assert(int_set_find(is,-4294967295));
        checkConsistency(is);
        ok();
        zfree(is);
    }

    printf("Upgrade from int32 to int64: "); {
        is = int_set_new();
        is = int_set_add(is,65535,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT32);
        is = int_set_add(is,4294967295,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT64);
        assert(int_set_find(is,65535));
        assert(int_set_find(is,4294967295));
        checkConsistency(is);
        zfree(is);

        is = int_set_new();
        is = int_set_add(is,65535,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT32);
        is = int_set_add(is,-4294967295,NULL);
        assert(intrev32ifbe(is->encoding) == INTSET_ENC_INT64);
        assert(int_set_find(is,65535));
        assert(int_set_find(is,-4294967295));
        checkConsistency(is);
        ok();
        zfree(is);
    }

    printf("Stress lookups: "); {
        long num = 100000, size = 10000;
        int i, bits = 20;
        long long start;
        is = create_int_set(bits,size);
        checkConsistency(is);

        start = usec();
        for (i = 0; i < num; i++) int_set_search(is,rand() % ((1<<bits)-1),NULL);
        printf("%ld lookups, %ld element set, %lldusec\n",
               num,size,usec()-start);
        zfree(is);
    }

    printf("Stress add+delete: "); {
        int i, v1, v2;
        is = int_set_new();
        for (i = 0; i < 0xffff; i++) {
            v1 = rand() % 0xfff;
            is = int_set_add(is,v1,NULL);
            assert(int_set_find(is,v1));

            v2 = rand() % 0xfff;
            is = int_set_remove(is,v2,NULL);
            assert(!int_set_find(is,v2));
        }
        checkConsistency(is);
        ok();
        zfree(is);
    }

    return 0;
}

int main() {
    test_api();
    return 0;
}