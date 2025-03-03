



#include "test/testhelp.h"
#include "test/testassert.h"
#include "thread_pool.h"
#include "ae/ae.h"
#include <time.h>
#include <unistd.h>

typedef struct  {
    int a;
    int b;
} add_info;
latte_error_t* test_add(void* arg, void** result) {
    add_info* info = (add_info*)arg;
    *result = (void*)(info->a + info->b);
    usleep(3000);
    return NULL;
}
aeEventLoop* loop;
int count = 300;
int callback_count = 0;
int all = 0;
void test_callback(latte_error_t* error, void* result) {
    all += (int)result;
    callback_count++;
    if (callback_count == count) {
        log_info(LATTE_LIB, "callback: %d", all);
        loop->stop = 1;
    }
    
}

void test_task_free(void* t) {
    latte_thread_task_t* task = (latte_thread_task_t*)t;
    zfree(task->arg);
}
int test_thread() {
    loop = aeCreateEventLoop(1024);
    latte_thread_pool_t* pool = thread_pool_new(1, 4, 10, 1000, loop);
    assert(pool != NULL);
    for(int i = 0; i < count; i++) {
        add_info* info = zmalloc(sizeof(add_info));
        info->a = 2 * i;
        info->b = 2 * i + 1;
        latte_thread_task_t* task = latte_thread_task_new(test_add, info, test_callback, test_task_free);
        thread_pool_submit(pool, task);
        if (i < 100 || i > 200) {
            usleep(500);
        } else {
            usleep(1000);
        }
        if (i % 10 == 0) {
            thread_pool_try_scaling(pool);
        }
    }
    aeMain(loop);
    return 1;
}

int test_api(void) {
    {
        log_init();
        assert(log_add_stdout(LATTE_LIB, LOG_INFO) == 1);
#ifdef LATTE_TEST
        // ..... private
#endif
        test_cond("test thread fuinction", 
            latte_test_function_time(test_thread) == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}