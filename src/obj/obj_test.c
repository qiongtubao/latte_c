//
// Created by dong on 23-5-22.
//

#include "test/testhelp.h"
#include "test/testassert.h"

#include <stdio.h>
#include "zmalloc/zmalloc.h"
#include "obj.h"
#include <unistd.h>

typedef struct User {
    int age;
} User;

void zfreeUser(User* user) {
    zfree(user);
}

int test_obj_local() {
    User* user = zmalloc(sizeof(User));
    user->age = 0;
    latteObj* obj =  latteObjCreate(user, zfreeUser);
    latteObjIncrRefCount(obj);
    latteObjDecrRefCount(obj);
    latteObjDecrRefCount(obj);
    return 1;
}




// 线程执行的函数
void* increment_counter(void* arg) {
    latteObj* task = (latteObj*)arg; // 获取线程需要执行的次数
    for(int i = 0; i < 1000; ++i) {
        latteObjIncrRefCount(task);
        latteObjDecrRefCount(task);
    }
    return NULL;
}

int test_obj_thread() {
    // 创建两个线程
    pthread_t thread1, thread2;
    User* user = zmalloc(sizeof(User));
    user->age = 0;
    latteObj* obj =  latteObjCreate(user, zfreeUser);

    if(pthread_create(&thread1, NULL, increment_counter, obj) != 0 ||
       pthread_create(&thread2, NULL, increment_counter, obj) != 0) {
        printf("\n Failed to create threads\n");
        return -1;
    }
    // assert( obj->refcount >= 1);
    // 等待两个线程结束
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    assert( atomic_load(&obj->refcount) == 1);
    latteObjDecrRefCount(obj);
    return 1;
}

int test_obj0() {
    atomic_uint ref_count = 1;
    printf("add %d\n", atomic_fetch_add(&ref_count, 1));
    printf("load %d\n", atomic_load(&ref_count));
    printf("sub %d\n", atomic_fetch_sub(&ref_count, 1));
    printf("load %d\n", atomic_load(&ref_count));
    return 1;
}

int test_api(void) {
    {
#ifdef LATTE_TEST
        // ..... private
#endif
        test_cond("obj function",
                  test_obj_local() == 1);
        test_cond("obj thread fuinction", 
            test_obj_thread() == 1);
        // test_cond("obj test fuinction", 
        //     test_obj0() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}