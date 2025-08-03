
#include "test/testhelp.h"
#include "test/testassert.h"

#include "list/list.h"
#include "time/localtime.h" 
#include "log/log.h"
#include "utils/utils.h"

#include "thread_job.h"
#include "thread_consumer_job.h"

void* add(int argc, void** args, latte_error_t* error) {
    latte_assert_with_info(argc == 2, "argc is not 2");
    latte_assert_with_info(args[0] != NULL, "args[0] is NULL");
    latte_assert_with_info(args[1] != NULL, "args[1] is NULL");
    long a = (long)args[0];
    long b = (long)args[1];
    zfree(args);
    return (void*)(a + b);
}



int thread_job_test() {
    long* args = zmalloc(sizeof(long) * 2);
        args[0] = (void*)(long)1;
        args[1] = (void*)(long)2;
    latte_job_t* job = latte_job_new(add, 2, args);
    latte_job_run(job);
    latte_assert_with_info(job->result == (void*)(long)3, "job->result is not 3");
    latte_assert_with_info(job->error.code == COk, "job->error.code is not COk");
    latte_job_delete(job);
    return 1;
}

void* test(int argc, void** args, latte_error_t* error) {
    return NULL;
}

int thread_consumer_job_test() {
    latte_thread_consumer_job_manager_t* manager = latte_thread_consumer_job_manager_new(5, 50);
    latte_thread_consumer_job_manager_adjust(manager, 5);
    for (long i = 0; i < 100000000; i++) {
        latte_job_t job;
        latte_job_init(&job, test, 0, NULL);
        latte_thread_consumer_job_manager_push(manager, &job);
    }
    latte_thread_consumer_job_manager_delete(manager);
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        log_module_init();
        assert(log_add_stdout("test", LOG_DEBUG) == 1);
        assert(log_add_stdout("latte_lib", LOG_DEBUG) == 1);
        test_cond("about thread_pool_test function", 
            thread_job_test() == 1);

        test_cond("about thread_consumer_job_test function", 
            thread_consumer_job_test() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}