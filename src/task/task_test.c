
#include "test/testhelp.h"
#include "test/testassert.h"

#include <stdio.h>
#include "task.h"
#include "zmalloc/zmalloc.h"
#include <pthread.h>

void hello(latteThreadJob* job) {
    printf("\n[thread]%s say hello\n", job->args[0]);
}

int test_task() {
    taskThread* thread = createTaskThread(1);
    startTaskThread(thread);
    latteThreadJob* job = createThreadJob(hello, NULL, 1, "latte");
    submitTask(thread, job);
    sleep(10);
    return 1;
}
int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("test task function", 
            test_task() == 1);
    
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}