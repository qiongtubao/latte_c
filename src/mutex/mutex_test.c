#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <unistd.h>
#include "mutex.h"
// 全局变量，需要被多个线程共享
int global_count = 0;
typedef struct pthread_task
{
    int timers;
    int global_count;
    latte_mutex* mutex;
} pthread_task;

// 线程执行的函数
void* increment_counter(void* arg) {
    pthread_task* task = (pthread_task*)arg; // 获取线程需要执行的次数
    for(int i = 0; i < task->timers; ++i) {
        // 加锁
        mutexLock(task->mutex);
        task->global_count++;
        // 解锁
        mutexUnlock(task->mutex);
        usleep(1000); // 模拟一些工作时间，非必须
    }
    return NULL;
}

int test_mutex_(latte_mutex* mutex) {
    // 创建两个线程
    pthread_t thread1, thread2;
    pthread_task task;
    task.mutex = mutex;
    task.timers = 100000;
    task.global_count = 0;
    if(pthread_create(&thread1, NULL, increment_counter, &task) != 0 ||
       pthread_create(&thread2, NULL, increment_counter, &task) != 0) {
        printf("\n Failed to create threads\n");
        return -1;
    }

    // 等待两个线程结束
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    assert(task.global_count == 200000);
    return 1;
}
int test_mutex() {
    latte_mutex mutex;
    assert(mutexInit(&mutex) == 0);
    test_mutex_(&mutex);
    mutexDestroy(&mutex);
    return 1;
}

int test_mutex_new() {
    latte_mutex* mutex = mutexCreate();
    test_mutex_(mutex);
    mutexRelease(mutex); 
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("about mutex function", 
            test_mutex() == 1);
        test_cond("about new mutex function", 
            test_mutex_new() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}