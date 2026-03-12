/**
 * @file queue.h
 * @brief 队列数据结构接口
 *        基于链表实现的先进先出（FIFO）队列，支持任意类型数据（void* 指针）。
 */
#ifndef __LATTE_QUEUE_H
#define __LATTE_QUEUE_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief 队列节点结构体
 */
typedef struct queue_node_t {
    void *data;             /**< 节点存储的数据指针 */
    struct queue_node_t *next; /**< 指向下一个节点的指针 */
} queue_node_t;

/**
 * @brief 队列结构体
 */
typedef struct queue_t {
    queue_node_t *head; /**< 队列头节点（出队端） */
    queue_node_t *tail; /**< 队列尾节点（入队端） */
    size_t size;        /**< 当前队列中的元素数量 */
} queue_t;

/**
 * @brief 创建新队列
 *        分配内存并初始化为空队列
 * @return 新建的队列指针；内存分配失败返回 NULL
 */
queue_t* queue_create(void);

/**
 * @brief 销毁队列
 *        释放队列及其所有节点的内存（不释放节点中的 data 指针）
 * @param queue 要销毁的队列指针（NULL 时安全跳过）
 */
void queue_destroy(queue_t *queue);

/**
 * @brief 入队操作：在队列尾部添加新元素
 * @param queue 目标队列指针
 * @param data  要入队的数据指针
 * @return 成功返回 true；queue 为 NULL 或内存分配失败返回 false
 */
bool queue_enqueue(queue_t *queue, void *data);

/**
 * @brief 出队操作：从队列头部移除并返回元素
 * @param queue 目标队列指针
 * @return 队列头部的数据指针；queue 为 NULL 或队列为空返回 NULL
 */
void* queue_dequeue(queue_t *queue);

/**
 * @brief 查看队列头部元素（不移除）
 * @param queue 目标队列指针
 * @return 队列头部的数据指针；queue 为 NULL 或队列为空返回 NULL
 */
void* queue_peek(queue_t *queue);

/**
 * @brief 判断队列是否为空
 * @param queue 目标队列指针（NULL 视为空队列）
 * @return 队列为空返回 true；否则返回 false
 */
bool queue_is_empty(queue_t *queue);

/**
 * @brief 获取队列中的元素数量
 * @param queue 目标队列指针（NULL 返回 0）
 * @return 队列中的元素数量
 */
size_t queue_size(queue_t *queue);

#endif /* __LATTE_QUEUE_H */
