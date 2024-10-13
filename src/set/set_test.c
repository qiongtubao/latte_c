#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <unistd.h>
#include "set.h"
#include "lockSet.h"
#include "avlSet.h"
#include "hashSet.h"
// 全局变量，需要被多个线程共享
int global_count = 0;
typedef struct pthread_task
{
    int timers;
    lockSet* lockset;
} pthread_task;

// 线程执行的函数
void* increment_counter(void* arg) {
    pthread_task *t = (pthread_task*) arg;
    lockSet* set = t->lockset;
    sds_t k1 = sds_new("1");
    sds_t k2 = sds_new("2");
    sds_t k3 = sds_new("3");

    //栈不能太大 
    sds* keys = zmalloc(sizeof(sds) * t->timers);
    // printf("keys before %p \n", keys);
    for(int i  = 0; i < t->timers; i++) {
        char buf[10];
        int size = sprintf(buf, "%d" , i);
        sds_t k4 = sds_new_len(buf, size);
        keys[i] = k4;
       
        // 添加一些值
        lockSetAdd(set, k1);
        lockSetAdd(set, k2);
        lockSetAdd(set, k3);
        
        // 检查是否包含
        lockSetContains(set, k2);
        // 删除一些值
        lockSetRemove(set, k2);
        // 再次检查是否包含
        lockSetContains(set, k2);
        lockSetContains(set, k3);
        lockSetContains(set, k1);

        lockSetAdd(set, keys[i]);
    }
    // printf("keys after %p \n", keys);
    // printf("keys %d %p \n", 0, keys[0]);
    sds_delete(k1);
    sds_delete(k2);
    sds_delete(k3);
    for(int i  = 0; i < t->timers; i++) {
        // printf("keys %d %s \n", i, keys[i]);
        lockSetRemove(set, keys[i]);
        sds_delete(keys[i]);
    }
    zfree(keys);
    pthread_exit(NULL);
    return NULL;
}

int test_lockSet_(lockSet* lockset) {
    // 创建两个线程
    pthread_t threads[10];
    pthread_task task;
    task.lockset = lockset;
    task.timers = 100000;
    for(int i = 0; i < 10; i++) {
        if (pthread_create(&threads[i], NULL, increment_counter, &task) != 0) {
            printf("\n Failed to create threads\n");
            return -1;
        }
    }

    for(int i = 0; i < 10; i++) {
        // 等待两个线程结束
        pthread_join(threads[i], NULL);
    }
    assert(lockSetSize(task.lockset) == 0);
    return 1;
}


int test_lock_set(set_t* s) {
    lockSet* lockset = lockSetCreate(s);
    test_lockSet_(lockset);
    lockSetRelease(lockset); 
    return 1;
}
int test_lockSet() {
    set_t* s = set_newHash(&sdsHashSetDictType);
    assert(test_lock_set(s) == 1);

    s = set_newAvl(&avlSetSdsType);
    assert(test_lock_set(s) == 1);
    
    return 1;
}
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

    Iterator* iterator = set_get_iterator(s1);
    int i = 0;
    sds_t keys[3] = {key, key1, key2};
    while (iteratorHasNext(iterator)) {
        void* node = iteratorNext(iterator);
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
    iteratorRelease(iterator);

    sds_delete(key);
    sds_delete(key1);
    sds_delete(key2);
    set_delete(s1);
    return 1;
}

int test_hash_api() {
    hashSet* set = hashSetCreate(&sdsHashSetDictType);
    sds_t key = sds_new("1");
    sds_t key1 = sds_new("key1");
    sds_t key2 = sds_new("key2");
    assert(!hashSetContains(set, key));
    assert(hashSetAdd(set, key));
    assert(!hashSetAdd(set, key));
    assert(hashSetContains(set, key));
    
    assert(hashSetAdd(set, key1));
    assert(hashSetAdd(set, key2));

    hashSetIterator* iterator =  hashSetGetIterator(set);
    hashSetNode* node;
    int i = 0;
    while ((node = hashSetNext(iterator)) != NULL) {
        i++;
    }
    assert(i == 3);
    hashSetReleaseIterator(iterator);
    assert(hashSetRemove(set, key));
    assert(!hashSetContains(set, key));
    hashSetRelease(set);
    sds_delete(key);


    return 1;
}

sds_t getHashNodeKey(void* node) {
    hashSetNode* n = (hashSetNode*)node;
    return n->key;
}
int test_hash_set_api() {
    set_t* s = set_newHash(&sdsHashSetDictType);
    return test_set_base(s, getHashNodeKey, 0);
}

int test_hash() {
    // assert(test_hash_api() == 1);
    assert(test_hash_set_api() == 1);
    return 1;
}
int test_avlset_api() {
    avlSet* s = avlSetCreate(&avlSetSdsType);
    sds_t key = sds_new("key");
    sds_t key1 = sds_new("key1");
    sds_t key2 = sds_new("key2");
    assert(avlSetContains(s, key) == 0);
    assert(avlSetSize(s) == 0);
    assert(avlSetAdd(s, key) == 1);
    assert(avlSetSize(s) == 1);
    assert(avlSetAdd(s, key) == 0);
    assert(avlSetSize(s) == 1);
    assert(avlSetContains(s, key) == 1);
    assert(avlSetContains(s, key) == 1);
    assert(avlSetRemove(s, key) == 1);
    assert(avlSetContains(s, key) == 0);
    assert(avlSetRemove(s, key) == 0);
    assert(avlSetSize(s) == 0);

    assert(avlSetAdd(s, key) == 1);
    assert(avlSetAdd(s, key1) == 1);
    assert(avlSetAdd(s, key2) == 1);

    sds_t keys[3] = {key, key1, key2};
    avlSetIterator* iter = avlSetGetAvlSetIterator(s);
    avlNode* node = NULL;
    int i = 0;
    while (avlSetIteratorHasNext(iter)) {
        node = avlSetIteratorNext(iter);
        assert(sds_cmp(node->key, keys[i]) == 0);
        i++;
    }
    
    assert(i == 3);
    avlSetIteratorRelease(iter);

    Iterator* iterator = avlSetGetIterator(s);
    i = 0;
    while (iteratorHasNext(iterator)) {
        node = iteratorNext(iterator);
        assert(sds_cmp(node->key, keys[i]) == 0);
        i++;
    }
    assert(i == 3);
    iteratorRelease(iterator);
    sds_delete(key);
    sds_delete(key1);
    sds_delete(key2);
    avlSetRelease(s);
    return 1;
}
sds_t getAvlNode(void* n) {
    avlNode* node = (avlNode*)n;
    return node->key;
}

int test_avlset_set_api() {
    set_t* s1 = set_newAvl(&avlSetSdsType);
    return test_set_base(s1, getAvlNode, 1);
}

int test_avlset() {
    assert(test_avlset_api() == 1);
    assert(test_avlset_set_api() == 1);
    
    return 1;
}
int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("about set function", 
            test_hash() == 1);
        test_cond("about lockSet function", 
            test_lockSet() == 1);
        test_cond("about avlSet function",
            test_avlset() == 1);

    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}