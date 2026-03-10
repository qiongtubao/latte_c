/**
 *  线程中
 *    2次释放同一个对象
 */
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>

void* threadFunc(void* arg) {
    int *array = (int*)malloc(10 * sizeof(int));
    
    // 使用 array...
    printf("Thread: Array allocated at %p\n", (void*)array);

    // 第一次释放
    free(array);
    printf("Thread: Array freed\n");

    // 故意再次释放同一块内存（导致 double free）
    free(array); 
    printf("Thread: Attempted to free the same array again\n");

    return NULL;
}

int main() {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, threadFunc, NULL);
    pthread_create(&t2, NULL, threadFunc, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    return 0;
}