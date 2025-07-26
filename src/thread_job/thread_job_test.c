
#include "test/testhelp.h"
#include "test/testassert.h"

#include "list/list.h"
#include "time/localtime.h" 
#include "log/log.h"
#include "utils/utils.h"

#include "thread_job.h"

void* add(int argc, void** args, latte_error_t* error) {
    latte_assert_with_info(argc == 2, "argc is not 2");
    latte_assert_with_info(args[0] != NULL, "args[0] is NULL");
    latte_assert_with_info(args[1] != NULL, "args[1] is NULL");
    long a = (long)args[0];
    long b = (long)args[1];
    return (void*)(a + b);
}



int thread_job_test() {
    latte_job_t* job = latte_job_new(add, 2, (void*[]){(void*)(long)1, (void*)(long)2});
    latte_job_run(job);
    latte_assert_with_info(job->result == (void*)(long)3, "job->result is not 3");
    latte_assert_with_info(job->error.code == COk, "job->error.code is not COk");
    latte_job_delete(job);
    return 1;
}


int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        log_init();
        assert(log_add_stdout("test", LOG_DEBUG) == 1);
        test_cond("about thread_pool_test function", 
            thread_job_test() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}