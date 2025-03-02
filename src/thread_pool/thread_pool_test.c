



#include "test/testhelp.h"
#include "test/testassert.h"
#include "thread_pool.h"
#include "ae/ae.h"

typedef struct  {
    int a;
    int b;
} add_info;
latte_error_t* test_add(void* arg, void** result) {
    add_info* info = (add_info*)arg;
    log_info(LATTE_LIB, "add result: %d + %d", info->a, info->b);
    *result = (void*)(info->a + info->b);
}
aeEventLoop* loop;
int all = 0;
void test_callback(latte_error_t* error, void* result) {
    log_info(LATTE_LIB, "callback: %d", (int)result);
    all += (int)result;
    if (all >= 6) {
        loop->stop = 1;
    }
    
}

void test_task_free(void* t) {
    latte_thread_task_t* task = (latte_thread_task_t*)t;
    zfree(task->arg);
}
int test_thread() {
    loop = aeCreateEventLoop(1024);
    latte_thread_pool_t* pool = thread_pool_new(1, 2, 10, 1000, loop);
    assert(pool != NULL);
    for(int i = 0; i < 6; i+=2) {
        add_info* info = zmalloc(sizeof(add_info));
        info->a = i;
        info->b = i + 1;
        latte_thread_task_t* task = latte_thread_task_new(test_add, info, test_callback, test_task_free);
        thread_pool_submit(pool, task);
    }
    aeMain(loop);
    return 1;
}

int test_api(void) {
    {
        log_init();
        assert(log_add_stdout(LATTE_LIB, LOG_DEBUG) == 1);
#ifdef LATTE_TEST
        // ..... private
#endif
        test_cond("test thread fuinction", 
            test_thread() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}