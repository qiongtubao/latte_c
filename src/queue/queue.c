/**
 * @file queue.c
 * @brief 队列数据结构实现文件
 * @details 实现队列相关的所有函数
 * @author latte_c
 * @date 2026-03-11
 */

#include "queue.h"
#include <stdlib.h>

/**
 * @brief 创建新队列
 * @details 分配内存并初始化一个新的队列结构
 * @return queue_t* 指向新创建队列的指针，失败返回NULL
 */
queue_t* queue_create(void) {
    queue_t *queue = (queue_t*)malloc(sizeof(queue_t));
    if (queue == NULL) {
        return NULL; /* 内存分配失败 */
    }

    /* 初始化队列为空状态 */
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;

    return queue;
}

/**
 * @brief 销毁队列
 * @details 释放队列及其所有节点占用的内存
 * @param queue 要销毁的队列指针
 */
void queue_destroy(queue_t *queue) {
    if (queue == NULL) {
        return; /* 空指针检查 */
    }

    /* 逐个释放队列中的节点 */
    while (!queue_is_empty(queue)) {
        queue_dequeue(queue);
    }

    /* 释放队列结构本身 */
    free(queue);
}

/**
 * @brief 入队操作
 * @details 在队列尾部添加新元素
 * @param queue 队列指针
 * @param data 要添加的数据指针
 * @return bool 成功返回true，失败返回false
 */
bool queue_enqueue(queue_t *queue, void *data) {
    if (queue == NULL) {
        return false; /* 空指针检查 */
    }

    /* 创建新节点 */
    queue_node_t *new_node = (queue_node_t*)malloc(sizeof(queue_node_t));
    if (new_node == NULL) {
        return false; /* 内存分配失败 */
    }

    /* 初始化新节点 */
    new_node->data = data;
    new_node->next = NULL;

    /* 如果队列为空，新节点既是头也是尾 */
    if (queue->size == 0) {
        queue->head = new_node;
        queue->tail = new_node;
    } else {
        /* 将新节点链接到队列尾部 */
        queue->tail->next = new_node;
        queue->tail = new_node;
    }

    queue->size++; /* 增加队列大小 */
    return true;
}

/**
 * @brief 出队操作
 * @details 从队列头部移除并返回元素
 * @param queue 队列指针
 * @return void* 队列头部的数据指针，队列为空返回NULL
 */
void* queue_dequeue(queue_t *queue) {
    if (queue == NULL || queue_is_empty(queue)) {
        return NULL; /* 空指针或空队列检查 */
    }

    /* 保存头节点和数据 */
    queue_node_t *old_head = queue->head;
    void *data = old_head->data;

    /* 移动头指针到下一个节点 */
    queue->head = old_head->next;

    /* 如果队列变为空，需要重置尾指针 */
    if (queue->head == NULL) {
        queue->tail = NULL;
    }

    /* 释放旧头节点内存 */
    free(old_head);
    queue->size--; /* 减少队列大小 */

    return data;
}

/**
 * @brief 查看队列头部元素
 * @details 返回队列头部元素但不移除它
 * @param queue 队列指针
 * @return void* 队列头部的数据指针，队列为空返回NULL
 */
void* queue_peek(queue_t *queue) {
    if (queue == NULL || queue_is_empty(queue)) {
        return NULL; /* 空指针或空队列检查 */
    }

    return queue->head->data; /* 返回头节点数据，不移除 */
}

/**
 * @brief 检查队列是否为空
 * @details 判断队列中是否有元素
 * @param queue 队列指针
 * @return bool 队列为空返回true，否则返回false
 */
bool queue_is_empty(queue_t *queue) {
    if (queue == NULL) {
        return true; /* 空指针视为空队列 */
    }

    return queue->size == 0; /* 大小为0表示空队列 */
}

/**
 * @brief 获取队列大小
 * @details 返回队列中元素的数量
 * @param queue 队列指针
 * @return size_t 队列中元素的数量
 */
size_t queue_size(queue_t *queue) {
    if (queue == NULL) {
        return 0; /* 空指针返回0 */
    }

    return queue->size; /* 返回队列当前大小 */
}