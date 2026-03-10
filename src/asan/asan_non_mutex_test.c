/**
 * 多线程同时修改同一个数据
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

int global_counter = 0;

void* increment_counter(void* arg) {
    for (int i = 0; i < 10000; ++i) {
        global_counter++;
    }
    return NULL;
}

int main() {
    pthread_t t1, t2, t3 , t4;

    // 创建两个线程，每个线程都会尝试增加 global_counter。
    if (pthread_create(&t1, NULL, increment_counter, NULL) != 0) {
        perror("Thread creation failed");
        return EXIT_FAILURE;
    }
    if (pthread_create(&t2, NULL, increment_counter, NULL) != 0) {
        perror("Thread creation failed");
        return EXIT_FAILURE;
    }
    // 创建两个线程，每个线程都会尝试增加 global_counter。
    if (pthread_create(&t3, NULL, increment_counter, NULL) != 0) {
        perror("Thread creation failed");
        return EXIT_FAILURE;
    }
    if (pthread_create(&t4, NULL, increment_counter, NULL) != 0) {
        perror("Thread creation failed");
        return EXIT_FAILURE;
    }

    // 等待所有线程完成。
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
    pthread_join(t4, NULL);
    printf("Final counter value: %d\n", global_counter);
    return 0;
}