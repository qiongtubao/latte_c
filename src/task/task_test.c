
#include "test/testhelp.h"
#include "test/testassert.h"

#include <stdio.h>
#include "task.h"
#include "zmalloc/zmalloc.h"
#include <pthread.h>

void hello(latteThreadJob* job) {
    printf("\n[thread]%s say hello\n", job->args[0]);
    job->result = "latte_callback";
}
void world(latteThreadJob* job) {
    printf("\n[thread]%s say callback \n", (char*)(job->result));
}

int test_task() {
    aeEventLoop* el = aeCreateEventLoop(1024);
    taskThread* thread = createTaskThread(1, el);
    startTaskThread(thread);
    latteThreadJob* job = createThreadJob(hello, world, 1, "latte");
    submitTask(thread, job);
    sleep(1);
    aeProcessEvents(el, AE_ALL_EVENTS|
                                   AE_CALL_BEFORE_SLEEP|
                                   AE_CALL_AFTER_SLEEP);
    aeDeleteEventLoop(el);
    stopTaskThread(thread);
    releaseTaskThread(thread);
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