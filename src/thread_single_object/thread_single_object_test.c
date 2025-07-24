
#include "../test/testhelp.h"
#include "../test/testassert.h"

#include "thread_single_object.h"

typedef struct tid_t {
    pthread_t tid;
} tid_t;

void* create_tid() {
    pthread_t tid = pthread_self();
    tid_t* tid_obj = (tid_t*)zmalloc(sizeof(tid_t));
    tid_obj->tid = tid;
    printf("create_tid thread tid: %lu\n", tid);
    return (void*)tid_obj;
}

void destroy_tid(void* arg) {
    tid_t* tid_obj = (tid_t*)arg;
    zfree(tid_obj);
}

void* get_tid(void* arg) {
    tid_t* tid_obj = (tid_t*)thread_single_object_get("tid");
    assert(tid_obj == NULL);
    return NULL;
}

int test_api_register() {
    thread_single_object_register(sds_new("tid"), destroy_tid);
    tid_t* tid_obj = (tid_t*)thread_single_object_get("tid");
    test_cond("thread_single_object register", tid_obj == NULL);
    tid_obj = create_tid();
    thread_single_object_set("tid", tid_obj);
    tid_obj = (tid_t*)thread_single_object_get("tid");
    test_cond("thread_single_object register", tid_obj != NULL);
    test_cond("thread_single_object register", tid_obj->tid == pthread_self());
    thread_single_object_delete("tid");

    pthread_t t1, t2;
    pthread_create(&t1, NULL, get_tid, NULL);
    pthread_create(&t2, NULL, get_tid, NULL);
    
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    
    return 1;
}
int test_api_unregister() {

    return 1;
}
int test_api() {
    {
        //TODO learn usage
        #ifdef LATTE_TEST
            // ..... private method test
        #endif
        thread_single_object_manager_init();
        test_cond("thread_single_object function", 
            test_api_register() == 1);
    }
    return 1;
}

int main() {
    test_api();
    return 0;
}