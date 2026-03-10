#include <stdio.h>
#include <time.h>
#include <stdatomic.h>
#include <pthread.h>

#define ITERATIONS 100000000

atomic_long atomic_value;
long regular_value = 0;

void* writer_thread_func(void* arg) {
    memory_order order = *(memory_order*)arg;
    for (int i = 0; i < ITERATIONS * 2; ++i) { //ITERATIONS * 2 避免在读取的时候 已经写结束了的情况
        atomic_fetch_add_explicit(&atomic_value, 1, order);
    }
    return NULL;
}

double measure_regular_read_time() {
    clock_t start, end;
    double cpu_time_used;

    start = clock();
    for (int i = 0; i < ITERATIONS; ++i) {
        volatile long unused = regular_value;
    }
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

    return cpu_time_used;
}

// 在 measure_regular_read_time 函数中不需要对 atomic_value 进行操作
// double measure_atomic_read_time_without_contention() {
//     clock_t start, end;
//     double cpu_time_used;

//     start = clock();
//     for (int i = 0; i < ITERATIONS; ++i) {
//         volatile long unused = atomic_load_explicit(&atomic_value, __ATOMIC_SEQ_CST);
//     }
//     end = clock();
//     cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

//     return cpu_time_used;
// }

double measure_atomic_read_time_with_contention(int thread_count, memory_order type) {
    // 测试开始前重置 atomic_value
    atomic_store(&atomic_value, 0);

    pthread_t threads[thread_count];
    for (int i = 0; i < thread_count; ++i) {
        pthread_create(&threads[i], NULL, writer_thread_func, (void*)&type);
    }

    clock_t start, end;
    double cpu_time_used;

    start = clock();
    for (int i = 0; i < ITERATIONS; ++i) {
        volatile long unused = atomic_load_explicit(&atomic_value, type);
    }
    end = clock();

    for (int i = 0; i < thread_count; ++i) {
        pthread_join(threads[i], NULL); // 确保所有写线程完成
    }

    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

    return cpu_time_used;
}


/**
 * build
 * gcc -std=c11 -pthread -o atomic_benchmark_test atomic_benchmark_test.c
 */
int main() {
    printf("Starting measurements...\n");

    atomic_init(&atomic_value, 0);

    double regular_time = measure_regular_read_time();
    
    printf("benchmark count: %d\n", ITERATIONS);
    printf("Regular read time: %f seconds\n", regular_time);
 
    // 测量有不同数量线程竞争下的原子读取时间
    int thread_counts[] = {0, 1, 2, 4, 8, 16};
    int types[] = { 
        __ATOMIC_RELAXED ,
        __ATOMIC_CONSUME ,
        __ATOMIC_ACQUIRE ,
        __ATOMIC_RELEASE ,
        __ATOMIC_ACQ_REL , 
        __ATOMIC_SEQ_CST 
    };
    char* typesnames[] = {
        "__ATOMIC_RELAXED" ,
        "__ATOMIC_CONSUME" ,
        "__ATOMIC_ACQUIRE" ,
        "__ATOMIC_RELEASE" ,
        "__ATOMIC_ACQ_REL" , 
        "__ATOMIC_SEQ_CST" 
    };
    for (int tc = 0; tc < sizeof(thread_counts)/sizeof(thread_counts[0]); ++tc) {
        for (int ti = 0; ti < sizeof(types)/sizeof(types[0]); ti++) {
            double atomic_with_contention_time = measure_atomic_read_time_with_contention(thread_counts[tc], types[ti]);
            printf("%d 线程写竞争atomic<long> 类型%s: %f seconds\n", thread_counts[tc], typesnames[ti], atomic_with_contention_time);
        }
    }
    return 0;
}