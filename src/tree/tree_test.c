#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <unistd.h>
#include "avl_tree.h"
#include "b_plus_tree.h"
#include "zmalloc/zmalloc.h"
#include "log/log.h"
#include <stdio.h>
typedef struct avlIntNode {
    avl_node_t node;
    void* value;
} avlIntNode;
avl_node_t* avl_tree_newIntNode(void* key, void* value) {
    avlIntNode* node = zmalloc(sizeof(avlIntNode));
    node->node.key = key;
    node->value = value;
    return (avl_node_t*)node;
}

void avl_node_tSetVal(avl_node_t* node_, void* value) {
    avlIntNode* node = (avlIntNode*)node_;
    if (node->value != NULL) zfree(node->value);
    node->value = value;
}
int KeyOperator(void* key, void* key1) {
    long k = (long)key;
    long k2 = (long)key1;
    long r = k - k2;
    if (r > 0) return 1;
    if (r < 0) return -1;
    return 0;
}

void avl_node_tRelease(avl_node_t* node_) {
    avlIntNode* node = (avlIntNode*)node_;
    zfree(node->value);
    zfree(node);
}
avl_tree_type_t treeType = {
    .create_node = avl_tree_newIntNode,
    .node_set_val = avl_node_tSetVal,
    .operator = KeyOperator,
    .release_node = avl_node_tRelease,
    .get_value = NULL
};
int test_avl_tree() {
    avl_tree_t* tree = avl_tree_new(&treeType);
    assert(avl_tree_get(tree, (void*)10L) == NULL);
    assert(avl_tree_put(tree, (void*)10L, NULL) == 1);
    assert(avl_tree_get(tree, (void*)10L) != NULL);
    assert(avl_tree_put(tree, (void*)10L, NULL) == 0);
    assert(avl_tree_get(tree, (void*)10L) != NULL);
    assert(avl_tree_put(tree, (void*)1L, NULL) == 1);
    assert(avl_tree_put(tree, (void*)5L, NULL) == 1);
    assert(avl_tree_put(tree, (void*)15L, NULL) == 1);

   

    // avl_tree_iterator_t* avl_tree_iterator_t = avl_tree_iterator_new(tree);
    long result[4] = {1L,5L,10L,15L};
    // int i = 0;
    // while(avl_tree_iterator_has_next(avl_tree_iterator_t)) {
    //     avl_node_t* node = avl_tree_iterator_tNext(avl_tree_iterator_t);
    //     assert((long)node->key == result[i]);
    //     i++;
    // }
    // assert(i == 4);
    // avl_tree_iterator_tRelease(avl_tree_iterator_t);

    latte_iterator_t* iterator = avl_tree_get_iterator(tree);
    int i = 0;
    while(latte_iterator_has_next(iterator)) {
        latte_pair_t* node = latte_iterator_next(iterator);
        assert((long)node->key == result[i]);
        i++;
    }
    assert(i == 4);
    latte_iterator_delete(iterator);

    assert(avl_tree_remove(tree, (void*)10L) == 1);
    assert(avl_tree_get(tree, (void*)10L) == NULL);
    return 1;
}

// =====
void traverseLeaves(b_plus_tree_node_t *node, sds prefix) {
    int i;
    
    if (!node->isLeaf) {
        for (i = 0; i <= node->numKeys; i++) {
            traverseLeaves(node->children[i], sds_cat_printf(sds_dup(prefix), "[%d]", i));
        }
    } else {
        for (i = 0; i < node->numKeys; i++) {
            printf("%s[%d]=(key=%d,value=%d) ", prefix, i, node->keys[i], node->data[i]);
        }
    }
}

void traverseIndex(b_plus_tree_node_t *node, sds index) {
    int i;
    
    if (!node->isLeaf) {
        for (i = 0; i < node->numKeys; i++) {
            printf("%s[%d]=(key=%d,value=%d) ", index, i, node->keys[i] ,node->data[i]);
        }
        for (i = 0; i <= node->numKeys; i++) {
            traverseIndex(node->children[i], sds_cat_printf(sds_dup(index), "[%d]", i));
        }
    } 
}

// 打印 B+树中的所有键值
void printBPlusTree(b_plus_tree_t *tree) {
    if (tree->root == NULL) {
        printf("The tree is empty.\n");
        return;
    }
    traverseIndex(tree->root, sds_new("index-root"));
    printf("\n");
    traverseLeaves(tree->root, sds_new("leaf-root"));
    printf("\n");
    
}
b_plus_tree_func_t btype = {
    .cmp = KeyOperator,
    .free_node = NULL
};



int test_bplus_tree() {
    b_plus_tree_t* tree = b_plus_tree_new(&btype, 16);
    b_plus_tree_insert(tree, (void*)10L, (void*)1L);
    b_plus_tree_insert(tree, (void*)3L, (void*)2L);
    b_plus_tree_insert(tree, (void*)20L, (void*)3L);
    b_plus_tree_insert(tree, (void*)5L, (void*)4L);
    b_plus_tree_insert(tree, (void*)15L, (void*)5L);
    b_plus_tree_insert(tree, (void*)1L, (void*)6L);
    b_plus_tree_insert(tree, (void*)21L, (void*)7L);

    latte_iterator_t* it;
    // it =  b_plus_tree_get_iterator(tree);
    long list[7] = {1,3,5,10,15,20,21};
    long i = 0;
    // while(latte_iterator_has_next(it)) {
    //     latte_pair_t* pair = latte_iterator_next(it);
    //     assert((long)pair->key == list[i++]);
    // }
    // latte_iterator_delete(it);


    void* result = b_plus_tree_find(tree, (void*)10L);
    assert((long)result == 1L);
    b_plus_tree_update(tree, (void*)10L, (void*)2L);
    result = b_plus_tree_find(tree, (void*)10L);
    assert((long)result == 2L);
    b_plus_tree_remove(tree, (void*)10L);
    result = b_plus_tree_find(tree, (void*)10L);
    assert(result == NULL);
    result = b_plus_tree_find(tree, (void*)15L);
    assert((long)result == 5L);

    b_plus_tree_delete(tree);

    tree = b_plus_tree_new(&btype, 2);
    b_plus_tree_insert(tree, (void*)10L, (void*)1L);
    b_plus_tree_insert(tree, (void*)3L, (void*)2L);
    b_plus_tree_insert(tree, (void*)20L, (void*)3L);
    b_plus_tree_insert(tree, (void*)5L, (void*)4L);
    printBPlusTree(tree);
    b_plus_tree_insert(tree, (void*)15L, (void*)5L);
    b_plus_tree_insert(tree, (void*)1L, (void*)6L);
    b_plus_tree_insert(tree, (void*)21L, (void*)7L);
    // for(long i = 3; i < 8; i++) {
    //     b_plus_tree_insert(tree, (void*)i, (void*)i);
    // }
    // b_plus_tree_insert(tree, (void*)1, (void*)1);
    printBPlusTree(tree);

    it =  b_plus_tree_get_iterator(tree);
 
    i = 0;
    while(latte_iterator_has_next(it)) {
        latte_pair_t* pair = latte_iterator_next(it);
        printf("%ld == %ld\n",(long)pair->key, list[i]);
        assert((long)pair->key == list[i++]);
    }
    assert(i == 7);
    latte_iterator_delete(it);
    for(int i = 0; i < 7; i++) {
        printf("remove key:%d\n", list[i]);
        assert(b_plus_tree_find(tree, (void*)list[i]) != NULL);
        assert(b_plus_tree_remove(tree, (void*)list[i]) == true);
        printf("remove end key:%d\n", list[i]);
    }
    printBPlusTree(tree);
    // assert(b_plus_tree_remove(tree, (void*)5) == true);
    // printBPlusTree(tree);

    // b_plus_tree_t* tree1 = b_plus_tree_new(&btype, 2);
    // b_plus_tree_insert(tree1, (void*)10L, (void*)1L);
    // // b_plus_tree_insert(tree1, (void*)3L, (void*)2L);
    // b_plus_tree_insert(tree1, (void*)20L, (void*)3L);
    // // b_plus_tree_insert(tree1, (void*)5L, (void*)4L);
    
    // b_plus_tree_insert(tree1, (void*)15L, (void*)5L);
    // // b_plus_tree_insert(tree1, (void*)1L, (void*)6L);
    // b_plus_tree_insert(tree1, (void*)21L, (void*)7L);    
    // printBPlusTree(tree1);
    return 1;
}

long random_long(int min, int max) {
    return (rand() % (max -min + 1)) + min;
}

uint64_t testHashCallback(const void *key) {
    return dict_gen_case_hash_function((unsigned char*)key, strlen((char*)key));
}

uint64_t testLongHashCallback(const void *key) {
    return (uint64_t)key;
}

int testLongCompareCallback(dict_t*privdata, const void *key1, const void *key2) {
    long l1,l2;
    DICT_NOTUSED(privdata);
    l1 = (long)key1;
    l2 = (long)key2;
    return l1 == l2;
}

dict_func_t dict_klong_vlong_func = {
    testLongHashCallback,
    NULL,
    NULL,
    testLongCompareCallback,
    NULL,
    NULL,
    NULL
};



int test_tree_random_add(int index) {
    vector_t* data = vector_new();
    b_plus_tree_t* tree = b_plus_tree_new(&btype, index + 2);
    avl_tree_t* avl_tree = avl_tree_new(&treeType);
    dict_t* d = dict_new(&dict_klong_vlong_func);
    for(int i = 0; i < 10000; i++) {
        long r = random_long(100,10000);
        if(NULL != dict_find(d, (void*)r)) continue;
        assert(DICT_OK == dict_add(d, (void*)r, (void*)r));
        vector_push(data, (void*)r);
        b_plus_tree_insert_or_update(tree, (void*)r, (void*)r); 
        avl_tree_put(avl_tree, (void*)r, (void*)r);
    }
    
    vector_sort(data, int64_cmp);
    int i = 0;
    latte_iterator_t* tree_it =  b_plus_tree_get_iterator(tree);
    while(latte_iterator_has_next(tree_it)) {
        latte_pair_t* p = latte_iterator_next(tree_it);
        assert(vector_get(data, i) == p->key);
        assert(vector_get(data, i) == p->value);
        i++;
    }
    latte_iterator_delete(tree_it);
    assert(i == vector_size(data));
    i = 0;
    tree_it = avl_tree_get_iterator(avl_tree);
    while(latte_iterator_has_next(tree_it)) {
        latte_pair_t* p = latte_iterator_next(tree_it);
        assert(vector_get(data, i) == p->key);
        // assert(vector_get(data, i) == p->value);
        i++;
    }
    latte_iterator_delete(tree_it);
    assert(i == vector_size(data));
    return 1;
}

int test_tree_random() {
    int i = 0;
    for(i = 0; i < 10; i++) {
        assert(test_tree_random_add(i) == 1);
    }
    return 1;
}

int test_tree_seq_add(int index) {

     b_plus_tree_t* tree = b_plus_tree_new(&btype, index + 2);
     avl_tree_t* avl_tree = avl_tree_new(&treeType);
    for(long j = 0; j < 10; j++) {
        for(long i = 0; i < 10000; i++) {
            b_plus_tree_insert(tree, (void*)(i + j * 10000), (void*)(i + j * 10000));
            avl_tree_put(avl_tree, (void*)(i + j * 10000), (void*)(i + j * 10000));
        }
    }
    
    latte_iterator_t* tree_it =  b_plus_tree_get_iterator(tree);
    long i = 0;
    while(latte_iterator_has_next(tree_it)) {
        latte_pair_t* p = latte_iterator_next(tree_it);
        printf("%d == %d\n", latte_pair_key(p), i);
        assert(i == latte_pair_key(p));
        i++;
    }
    assert(10 * 10000 == i);
    latte_iterator_delete(tree_it);

    tree_it = avl_tree_get_iterator(avl_tree);
    i = 0;
    while(latte_iterator_has_next(tree_it)) {
        latte_pair_t* p = latte_iterator_next(tree_it);
        printf("%d == %d\n", latte_pair_key(p), i);
        assert(i == latte_pair_key(p));
        i++;
    }
    assert(10 * 10000 == i);
    latte_iterator_delete(tree_it);
    return 1;
}

int test_tree_run() {
    int i = 0;
    // for(i = 0; i < 10; i++) {
        assert(test_tree_seq_add(0) == 1);
    // }
    return 1;
}



int test_api(void) {
    log_module_init();
    assert(log_add_stdout(LATTE_LIB, LOG_DEBUG) == 1);
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("about avl tree function", 
            test_avl_tree() == 1);
        test_cond("about bplus tree function", 
            test_bplus_tree() == 1);
        test_cond("about tree  run function",
            test_tree_run() == 1);  
        test_cond("about tree  random function",
            test_tree_random() == 1);
          
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}