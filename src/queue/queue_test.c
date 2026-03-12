/**
 * @file queue_test.c
 * @brief 队列数据结构测试文件
 * @details 测试队列的各种功能和边界情况
 * @author latte_c
 * @date 2026-03-11
 */

#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/**
 * @brief 测试队列创建和销毁功能
 * @details 测试基本的队列生命周期管理
 */
static void test_queue_create_destroy() {
    printf("Testing queue create/destroy... ");

    /* 测试创建队列 */
    queue_t *queue = queue_create();
    assert(queue != NULL);
    assert(queue_is_empty(queue) == true);
    assert(queue_size(queue) == 0);

    /* 测试销毁队列 */
    queue_destroy(queue);

    /* 测试销毁空指针（应该安全） */
    queue_destroy(NULL);

    printf("PASSED\n");
}

/**
 * @brief 测试入队功能
 * @details 测试向队列中添加元素
 */
static void test_queue_enqueue() {
    printf("Testing queue enqueue... ");

    queue_t *queue = queue_create();
    assert(queue != NULL);

    /* 测试入队操作 */
    int data1 = 1, data2 = 2, data3 = 3;
    assert(queue_enqueue(queue, &data1) == true);
    assert(queue_size(queue) == 1);
    assert(queue_is_empty(queue) == false);

    assert(queue_enqueue(queue, &data2) == true);
    assert(queue_size(queue) == 2);

    assert(queue_enqueue(queue, &data3) == true);
    assert(queue_size(queue) == 3);

    /* 测试空指针入队 */
    assert(queue_enqueue(NULL, &data1) == false);

    queue_destroy(queue);
    printf("PASSED\n");
}

/**
 * @brief 测试出队功能
 * @details 测试从队列中移除元素
 */
static void test_queue_dequeue() {
    printf("Testing queue dequeue... ");

    queue_t *queue = queue_create();
    assert(queue != NULL);

    /* 测试空队列出队 */
    assert(queue_dequeue(queue) == NULL);
    assert(queue_dequeue(NULL) == NULL);

    /* 添加测试数据 */
    int data1 = 1, data2 = 2, data3 = 3;
    queue_enqueue(queue, &data1);
    queue_enqueue(queue, &data2);
    queue_enqueue(queue, &data3);

    /* 测试出队操作（FIFO顺序） */
    int *result1 = (int*)queue_dequeue(queue);
    assert(result1 != NULL && *result1 == 1);
    assert(queue_size(queue) == 2);

    int *result2 = (int*)queue_dequeue(queue);
    assert(result2 != NULL && *result2 == 2);
    assert(queue_size(queue) == 1);

    int *result3 = (int*)queue_dequeue(queue);
    assert(result3 != NULL && *result3 == 3);
    assert(queue_size(queue) == 0);
    assert(queue_is_empty(queue) == true);

    /* 再次测试空队列出队 */
    assert(queue_dequeue(queue) == NULL);

    queue_destroy(queue);
    printf("PASSED\n");
}

/**
 * @brief 测试查看队列头部元素功能
 * @details 测试不移除元素的情况下查看头部元素
 */
static void test_queue_peek() {
    printf("Testing queue peek... ");

    queue_t *queue = queue_create();
    assert(queue != NULL);

    /* 测试空队列查看 */
    assert(queue_peek(queue) == NULL);
    assert(queue_peek(NULL) == NULL);

    /* 添加测试数据 */
    int data1 = 1, data2 = 2;
    queue_enqueue(queue, &data1);
    queue_enqueue(queue, &data2);

    /* 测试查看头部元素（不移除） */
    int *result = (int*)queue_peek(queue);
    assert(result != NULL && *result == 1);
    assert(queue_size(queue) == 2); /* 大小不变 */

    /* 再次查看应该返回相同元素 */
    result = (int*)queue_peek(queue);
    assert(result != NULL && *result == 1);

    queue_destroy(queue);
    printf("PASSED\n");
}

/**
 * @brief 测试队列为空判断功能
 * @details 测试各种情况下的空队列判断
 */
static void test_queue_is_empty() {
    printf("Testing queue is_empty... ");

    /* 测试空指针 */
    assert(queue_is_empty(NULL) == true);

    queue_t *queue = queue_create();
    assert(queue != NULL);

    /* 测试新创建的空队列 */
    assert(queue_is_empty(queue) == true);

    /* 添加元素后测试 */
    int data = 1;
    queue_enqueue(queue, &data);
    assert(queue_is_empty(queue) == false);

    /* 移除元素后测试 */
    queue_dequeue(queue);
    assert(queue_is_empty(queue) == true);

    queue_destroy(queue);
    printf("PASSED\n");
}

/**
 * @brief 测试队列大小获取功能
 * @details 测试各种操作下的队列大小变化
 */
static void test_queue_size() {
    printf("Testing queue size... ");

    /* 测试空指针 */
    assert(queue_size(NULL) == 0);

    queue_t *queue = queue_create();
    assert(queue != NULL);

    /* 测试初始大小 */
    assert(queue_size(queue) == 0);

    /* 测试入队后大小变化 */
    int data1 = 1, data2 = 2, data3 = 3;
    queue_enqueue(queue, &data1);
    assert(queue_size(queue) == 1);

    queue_enqueue(queue, &data2);
    assert(queue_size(queue) == 2);

    queue_enqueue(queue, &data3);
    assert(queue_size(queue) == 3);

    /* 测试出队后大小变化 */
    queue_dequeue(queue);
    assert(queue_size(queue) == 2);

    queue_dequeue(queue);
    assert(queue_size(queue) == 1);

    queue_dequeue(queue);
    assert(queue_size(queue) == 0);

    queue_destroy(queue);
    printf("PASSED\n");
}

/**
 * @brief 测试队列的FIFO特性
 * @details 验证队列先进先出的核心特性
 */
static void test_queue_fifo() {
    printf("Testing queue FIFO behavior... ");

    queue_t *queue = queue_create();
    assert(queue != NULL);

    /* 创建测试字符串数组 */
    char *strings[] = {"first", "second", "third", "fourth"};
    int count = sizeof(strings) / sizeof(strings[0]);

    /* 按顺序入队 */
    for (int i = 0; i < count; i++) {
        queue_enqueue(queue, strings[i]);
    }

    /* 按FIFO顺序出队并验证 */
    for (int i = 0; i < count; i++) {
        char *result = (char*)queue_dequeue(queue);
        assert(result != NULL);
        assert(strcmp(result, strings[i]) == 0);
    }

    assert(queue_is_empty(queue) == true);

    queue_destroy(queue);
    printf("PASSED\n");
}

/**
 * @brief 主测试函数
 * @details 运行所有队列测试
 * @return int 测试结果，0表示成功
 */
int main() {
    printf("=== Queue Data Structure Tests ===\n");

    /* 运行所有测试 */
    test_queue_create_destroy();
    test_queue_enqueue();
    test_queue_dequeue();
    test_queue_peek();
    test_queue_is_empty();
    test_queue_size();
    test_queue_fifo();

    printf("=== All tests PASSED! ===\n");
    return 0;
}