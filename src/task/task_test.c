/*
 * task_test.c - task 模块实现文件
 * 
 * Latte C 库组件实现
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */


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
    ae_event_loop_t* el = ae_event_loop_new(1024);
    taskThread* thread = createTaskThread(1, el);
    startTaskThread(thread);
    latteThreadJob* job = createThreadJob(hello, world, 1, "latte");
    submitTask(thread, job);
    sleep(1);
    ae_process_events(el, AE_ALL_EVENTS|
                                   AE_CALL_BEFORE_SLEEP|
                                   AE_CALL_AFTER_SLEEP);
    ae_event_loop_delete(el);
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