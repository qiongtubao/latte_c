
#include "test/testhelp.h"
#include "test/testassert.h"

#include <stdio.h>
#include "zmalloc/zmalloc.h"
#include "async.h"


void hello(latteThreadJob* job) {
    printf("\n[thread]%s say hello\n", job->args[0]);
    job->result = "latte_callback";
}


void hello2(latteThreadJob* job) {
    printf("\n[thread]%s say hello2\n", job->args[0]);
    job->result = "latte_callback2";
}
void world(latteThreadJob* job) {
    printf("\n[thread]%s say callback \n", (char*)(job->result));
}

int test_normal_task(aeEventLoop* el, taskThread* task_thread) {
    
    latteThreadJob* job = createThreadJob(hello, NULL, 1, "latte");
    asyncTask* task = createAsyncBasicTask(job);
    asyncRun(task_thread, task);
    sleep(1);
    aeProcessEvents(el, AE_ALL_EVENTS|
                                   AE_CALL_BEFORE_SLEEP|
                                   AE_CALL_AFTER_SLEEP);
    asyncWait(task);
    void* result = getBasicTaskResult(task);
    printf("\n[thread]%s say callback \n", (char*)(result));
    return 1;
}

int test_null_series_task(aeEventLoop* el, taskThread* task_thread) {
    seriesTask* task = createSeriesTask();
    // latteThreadJob* job1 = createThreadJob(hello, NULL, 1, "job1");
    // latteThreadJob* job2 = createThreadJob(hello2, NULL, 1, "job2");
    // task = addSeriesTask(task, job1);
    // task = addSeriesTask(task, job2);
    asyncRun(task_thread, task);
    printf("%d \n", task->task.status);
    assert(task->task.status == DONE_TASK_STATUS);
    return 1;
}

int test_series_one_task(aeEventLoop* el, taskThread* task_thread) {
    seriesTask* task = createSeriesTask();
    latteThreadJob* job = createThreadJob(hello, NULL, 1, "latte");
    asyncTask* task1 = createAsyncBasicTask( job);
    addSeriesTask(task, task1);
    asyncRun(task_thread, task);
    sleep(1);
    aeProcessEvents(el, AE_ALL_EVENTS|
                                   AE_CALL_BEFORE_SLEEP|
                                   AE_CALL_AFTER_SLEEP);
    asyncWait(task);
    void* result = getBasicTaskResult(task1);
    printf("\n[thread]%s say callback \n", (char*)(result));
    return 1;
}

int test_series_two_task(aeEventLoop* el, taskThread* task_thread) {
    seriesTask* task = createSeriesTask(1);
    latteThreadJob* job = createThreadJob(hello, NULL, 1, "latte");
    latteThreadJob* job2 = createThreadJob(hello2, NULL, 2, "t2");
    asyncTask* task1 = createAsyncBasicTask(job);
    asyncTask* task2 = createAsyncBasicTask(job2);
    addSeriesTask(task, task1);
    addSeriesTask(task, task2);
    asyncRun(task_thread, task);
    sleep(1);
    aeProcessEvents(el, AE_ALL_EVENTS|
                                   AE_CALL_BEFORE_SLEEP|
                                   AE_CALL_AFTER_SLEEP);
    assert(task1->status == DONE_TASK_STATUS);
    assert(task2->status == DOING_TASK_STATUS);
    void* result = getBasicTaskResult(task1);
    printf("\n[thread]%s say callback \n", (char*)(result));
    assert(task2->status == DOING_TASK_STATUS);

    sleep(1);
    aeProcessEvents(el, AE_ALL_EVENTS|
                                   AE_CALL_BEFORE_SLEEP|
                                   AE_CALL_AFTER_SLEEP);
    asyncWait(task);
    assert(task2->status == DONE_TASK_STATUS);
    result = getBasicTaskResult(task2);
    printf("\n[thread]%s say2 callback \n", (char*)(result));
    return 1;
}

void stop(latteThreadJob* job) {
    
}

void stopTask(asyncTask* task) {
    
}

int test_series_two_task_stop(aeEventLoop* el, taskThread* task_thread) {
    seriesTask* task = createSeriesTask();
    latteThreadJob* job = createThreadJob(hello, stop, 1, "latte");
    latteThreadJob* job2 = createThreadJob(hello2, stop, 2, "t2");
    asyncTask* task1 = createAsyncBasicTask(job);
    task1->cb = stopTask;
    asyncTask* task2 = createAsyncBasicTask(job2);
    task2->cb = stopTask;
    addSeriesTask(task, task1);
    addSeriesTask(task, task2);
    asyncRun(task_thread, task);
    sleep(1);
    aeProcessEvents(el, AE_ALL_EVENTS|
                                   AE_CALL_BEFORE_SLEEP|
                                   AE_CALL_AFTER_SLEEP);
    assert(task1->status == DONE_TASK_STATUS);
    assert(task2->status == WAIT_TASK_STATUS);
    continueNextTask(task1);
    assert(task2->status == DOING_TASK_STATUS);
    sleep(1);
    aeProcessEvents(el, AE_ALL_EVENTS|
                                   AE_CALL_BEFORE_SLEEP|
                                   AE_CALL_AFTER_SLEEP);
    
    assert(task2->status == DONE_TASK_STATUS);
    void* result = getBasicTaskResult(task2);
    printf("\n[thread]%s say2 callback \n", (char*)(result));
    assert(task->task.status == DOING_TASK_STATUS);
    continueNextTask(task2);
    assert(task->task.status == DONE_TASK_STATUS);
    return 1;
}

int test_series_one_task_add_task(aeEventLoop* el, taskThread* task_thread) {
    seriesTask* task = createSeriesTask();
    latteThreadJob* job = createThreadJob(hello, stop, 1, "latte");
    latteThreadJob* job2 = createThreadJob(hello2, stop, 2, "t2");
    asyncTask* task1 = createAsyncBasicTask(job);
    asyncTask* task2 = createAsyncBasicTask(job2);
    addSeriesTask(task, task1);
    asyncRun(task_thread, task);
    addSeriesTask(task, task2);
    sleep(1);
    aeProcessEvents(el, AE_ALL_EVENTS|
                                   AE_CALL_BEFORE_SLEEP|
                                   AE_CALL_AFTER_SLEEP);
    assert(task1->status == DONE_TASK_STATUS);
    assert(task2->status == DOING_TASK_STATUS);
    sleep(1);
    aeProcessEvents(el, AE_ALL_EVENTS|
                                   AE_CALL_BEFORE_SLEEP|
                                   AE_CALL_AFTER_SLEEP);
    asyncWait(task);
    assert(task2->status == DONE_TASK_STATUS);
    void* result = getBasicTaskResult(task2);
    printf("\n[thread]%s say2 callback \n", (char*)(result));
    return 1;
}

void sleep10(latteThreadJob* job) {
    sleep(10);
    printf("\n[thread]%s say hello\n", job->args[0]);
    job->result = "latte_callback";
}


void sleep2(latteThreadJob* job) {
    sleep(2);
    printf("\n[thread]%s say hello2\n", job->args[0]);
    job->result = "latte_callback2";
}


int test_parallel(aeEventLoop* el, taskThread* task_thread) {
    parallelTask* task = createParallelTask();
    latteThreadJob* job = createThreadJob(sleep10, stop, 1, "latte");
    latteThreadJob* job2 = createThreadJob(sleep2, stop, 2, "t2");
    asyncTask* task1 = createAsyncBasicTask(job);
    asyncTask* task2 = createAsyncBasicTask(job2);
    addParallelTask(task,sds_new("task1"), task1);
    addParallelTask(task, sds_new("task2"), task2);
    asyncRun(task_thread, task);
    assert(task1->status == DOING_TASK_STATUS);
    assert(task2->status == DOING_TASK_STATUS);
    sleep(3);
    aeProcessEvents(el, AE_ALL_EVENTS|
                                   AE_CALL_BEFORE_SLEEP|
                                   AE_CALL_AFTER_SLEEP);
    assert(task2->status == DONE_TASK_STATUS);
    assert(task1->status == DOING_TASK_STATUS);
    sleep(8);
    aeProcessEvents(el, AE_ALL_EVENTS|
                                   AE_CALL_BEFORE_SLEEP|
                                   AE_CALL_AFTER_SLEEP);
    assert(task1->status == DONE_TASK_STATUS);
    assert(task->task.status == DONE_TASK_STATUS);
    return 1;
}


int test_parallel_add_some_task(aeEventLoop* el, taskThread* task_thread) {
    parallelTask* task = createParallelTask();
    latteThreadJob* job = createThreadJob(sleep10, stop, 1, "latte");
    latteThreadJob* job2 = createThreadJob(sleep2, stop, 2, "t2");
    latteThreadJob* job3 = createThreadJob(hello, stop, 1, "?");
    asyncTask* task1 = createAsyncBasicTask(job);
    asyncTask* task2 = createAsyncBasicTask(job2);
    asyncTask* task3 = createAsyncBasicTask(job3);
    addParallelTask(task,sds_new("task1"), task1);
    addParallelTask(task, sds_new("task2"), task2);
    asyncRun(task_thread, task);
    assert(task1->status == DOING_TASK_STATUS);
    assert(task2->status == DOING_TASK_STATUS);
    addParallelTask(task, sds_new("task1"), task3);
    assert(task3->status == WAIT_TASK_STATUS);
    sleep(3);
    aeProcessEvents(el, AE_ALL_EVENTS|
                                   AE_CALL_BEFORE_SLEEP|
                                   AE_CALL_AFTER_SLEEP);
    assert(task2->status == DONE_TASK_STATUS);
    assert(task1->status == DOING_TASK_STATUS);
    sleep(8);
    aeProcessEvents(el, AE_ALL_EVENTS|
                                   AE_CALL_BEFORE_SLEEP|
                                   AE_CALL_AFTER_SLEEP);
    assert(task1->status == DONE_TASK_STATUS);
    assert(task3->status == DOING_TASK_STATUS);
    sleep(1);
    aeProcessEvents(el, AE_ALL_EVENTS|
                                   AE_CALL_BEFORE_SLEEP|
                                   AE_CALL_AFTER_SLEEP);
    assert(task3->status == DONE_TASK_STATUS);
    assert(task->task.status == DONE_TASK_STATUS);
    return 1;
}

int test_parallel_add_differ_task(aeEventLoop* el, taskThread* task_thread) {
    parallelTask* task = createParallelTask();
    latteThreadJob* job = createThreadJob(sleep10, stop, 1, "latte");
    latteThreadJob* job2 = createThreadJob(sleep2, stop, 2, "t2");
    latteThreadJob* job3 = createThreadJob(hello, stop, 1, "?");
    asyncTask* task1 = createAsyncBasicTask(job);
    asyncTask* task2 = createAsyncBasicTask(job2);
    asyncTask* task3 = createAsyncBasicTask(job3);
    addParallelTask(task,sds_new("task1"), task1);
    addParallelTask(task, sds_new("task2"), task2);
    asyncRun(task_thread, task);
    assert(task1->status == DOING_TASK_STATUS);
    assert(task2->status == DOING_TASK_STATUS);
    addParallelTask(task, sds_new("task3"), task3);
    assert(task3->status == DOING_TASK_STATUS);
    sleep(1);
    aeProcessEvents(el, AE_ALL_EVENTS|
                                   AE_CALL_BEFORE_SLEEP|
                                   AE_CALL_AFTER_SLEEP);
    assert(task3->status == DONE_TASK_STATUS);
    sleep(3);
    aeProcessEvents(el, AE_ALL_EVENTS|
                                   AE_CALL_BEFORE_SLEEP|
                                   AE_CALL_AFTER_SLEEP);
    assert(task2->status == DONE_TASK_STATUS);
    assert(task1->status == DOING_TASK_STATUS);
    sleep(8);
    aeProcessEvents(el, AE_ALL_EVENTS|
                                   AE_CALL_BEFORE_SLEEP|
                                   AE_CALL_AFTER_SLEEP);
    assert(task1->status == DONE_TASK_STATUS);
    assert(task->task.status == DONE_TASK_STATUS);
    return 1;
}

int test_series_add_parallel_task(aeEventLoop* el, taskThread* task_thread) {
    seriesTask* task = createSeriesTask();
    parallelTask* task1 = createParallelTask();
    parallelTask* task2 = createParallelTask();
    addSeriesTask(task, task1);
    addSeriesTask(task, task2);
    asyncRun(task_thread, task);
    printf("task1 status %d \n", task1->task.status);
    printf("task2 status %d \n", task2->task.status);
    printf("task2 parent status %p %d \n",task2->task.parent, task2->task.parent->status);
    assert(task->task.status == DONE_TASK_STATUS);
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        aeEventLoop* el = aeCreateEventLoop(1024);
        taskThread* task_thread = createTaskThread(3, el);
        startTaskThread(task_thread);
        test_cond("test normal task function", 
            test_normal_task(el, task_thread) == 1);
        test_cond("test normal task function", 
            test_null_series_task(el, task_thread) == 1);
        test_cond("test test_series_one_task function",
            test_series_one_task(el, task_thread) == 1);
        test_cond("test_series_two_task function",
            test_series_two_task(el, task_thread) == 1);
        test_cond("test_series_two_task stop function",
            test_series_two_task_stop(el, task_thread));
        test_cond("test series two task add task function",
            test_series_one_task_add_task(el, task_thread));


        test_cond("test series two task parallel",
            test_parallel(el, task_thread));
        test_cond("test series two task parallel add same task",
            test_parallel_add_some_task(el, task_thread));
        test_cond("test series two task parallel add differ task",
            test_parallel_add_differ_task(el, task_thread));

        test_cond("test series two task parallel add differ task",
            test_series_add_parallel_task(el, task_thread));
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}